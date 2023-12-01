#include <cstdio>
#include <iostream>
#include <string.h>
#include <fcntl.h>

using namespace std;

#include <sys/unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#define LISTEN_BACKLOG 15	//일반적인 connection의 setup은 client가 connect()를 사용하여 connection request를 전송하면 server는 accept()를 사용하여 connection을 받아들입니다. 
							//그런데 만약 server가 다른 작업을 진행 중이면 accept()를 할 수 없는 경우가 발생하고 이 경우 connection request는 queue에서 대기하는데 backlog는 이와 같이 accept() 없이 대기 가능한 connection request의 최대 개수입니다. 
							//보통 5정도의 value를 사용하며 만약 아주 큰 값을 사용하면 kernel의 resource를 소모합니다.
							//따라서, 접속 가능한 클라이언트의 최대수가 아닌 연결을 기다리는 클라이언트의 최대수입니다

int main()
{
    printf("hello from Leviathan_for_Linux!\n");
	int						error_check;

	// 소켓 생성
	int						server_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
	{
		printf("socket() error\n");
		return 0;
	}

	// server fd Non-Blocking Socket으로 설정. Edge Trigger 사용하기 위해 설정. 
	int						flags = fcntl(server_fd, F_GETFL);
	flags					|= O_NONBLOCK;
	if (fcntl(server_fd, F_SETFL, flags) < 0)
	{
		printf("server_fd fcntl() error\n");
		return 0;
	}

	// 소켓 옵션 설정.
	// Option -> SO_REUSEADDR : 비정상 종료시 해당 포트 재사용 가능하도록 설정
	int						option = true;
	error_check				= setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
	if (error_check < 0)
	{
		printf("setsockopt() error[%d]\n", error_check);
		close(server_fd);
		return 0;
	}

	// 소켓 속성 설정
	struct sockaddr_in		mSockAddr;
	memset(&mSockAddr, 0, sizeof(mSockAddr));
	mSockAddr.sin_family	= AF_INET;
	mSockAddr.sin_port		= htons(12345);
	mSockAddr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY : 사용가능한 랜카드 IP 사용

	// 소켓 속성과 소켓 fd 연결
	error_check				= bind(server_fd, (struct sockaddr*)&mSockAddr, sizeof(mSockAddr));
	if (error_check < 0)
	{
		printf("bind() error[%d]\n", error_check);
		close(server_fd);
		return 0;
	}

	// 응답 대기
	if (listen(server_fd, LISTEN_BACKLOG) < 0)
	{
		printf("listen() error\n");
		close(server_fd);
		return 0;
	}

	// Epoll fd 생성
	int						epoll_fd = epoll_create(1024);	// size 만큼의 커널 폴링 공간을 만드는 함수
	if (epoll_fd < 0)
	{
		printf("epoll_create() error\n");
		close(server_fd);
		return 0;
	}

	// server fd, epoll에 등록
	// EPOLLET => Edge Trigger 사용
	// 레벨트리거와 에지 트리거를 소켓 버퍼에 대응하면, 소켓 버퍼의 레벨 즉 데이터의 존재 유무를 가지고 판단하는 것이
	// 레벨트리거 입니다.즉 읽어서 해당 처리를 하였다 하더라도 덜 읽었으면 계속 이벤트가 발생하겠지요.예를 들어
	// 1000바이트가 도착했는데 600바이트만 읽었다면 레벨 트리거에서는 계속 이벤트를 발생시킵니다.그러나
	// 에지트리거에서는 600바이트만 읽었다 하더라도 더 이상 이벤트가 발생하지 않습니다.왜냐하면 읽은 시점을 기준으로
	// 보면 더이상의 상태 변화가 없기 때문이죠..LT 또는 ET는 쉽게 옵션으로 설정 가능합니다.
	// 참고로 select / poll은 레벨트리거만 지원합니다.
	struct epoll_event		events;
	events.events			= EPOLLIN | EPOLLET;
	events.data.fd			= server_fd;
	
	/* server events set(read for accept) */
	// epoll_ctl : epoll이 관심을 가져주기 바라는 FD와 그 FD에서 발생하는 event를 등록하는 인터페이스.
	// EPOLL_CTL_ADD : 관심있는 파일디스크립트 추가
	// EPOLL_CTL_MOD : 기존 파일 디스크립터를 수정
	// EPOLL_CTL_DEL : 기존 파일 디스크립터를 관심 목록에서 삭제
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &events) < 0)
	{
		printf("epoll_ctl() error\n");
		close(server_fd);
		close(epoll_fd);
		return 0;
	}

	// epoll wait.
	// 관심있는 fd들에 무슨일이 일어났는지 조사.
	// 사건들의 리스트를 epoll_events[]의 배열로 전달.
	// 리턴값은 사건들의 개수, 사건의 최대 개수는 MAX_EVENTS 만큼
	// timeout	-1 		--> 영원히 사건 기다림(blocking)
	// 			0  		--> 사건 기다리지 않음.
	//			0 < n 	--> (n)ms 만큼 대기
	int						MAX_EVENTS = 1024;
	struct epoll_event		epoll_events[MAX_EVENTS];
	int						event_count;
	int						timeout = -1;

	while (true)
	{
		event_count			= epoll_wait(epoll_fd, epoll_events, MAX_EVENTS, timeout);
		printf("event count[%d]\n", event_count);

		if (event_count < 0)
		{
			printf("epoll_wait() error [%d]\n", event_count);
			return 0;
		}

		for (int i = 0; i < event_count; i++)
		{
			// Accept
			if (epoll_events[i].data.fd == server_fd)
			{
				int			client_fd;
				int			client_len;
				struct sockaddr_in	client_addr;
				
				printf("User Accept\n");
				client_len	= sizeof(client_addr);
				client_fd	= accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);

				// client fd Non-Blocking Socket으로 설정. Edge Trigger 사용하기 위해 설정. 
				int						flags = fcntl(client_fd, F_GETFL);
				flags |= O_NONBLOCK;
				if (fcntl(client_fd, F_SETFL, flags) < 0)
				{
					printf("client_fd[%d] fcntl() error\n", client_fd);
					return 0;
				}

				if (client_fd < 0)
				{
					printf("accept() error [%d]\n", client_fd);
					continue;
				}

				// 클라이언트 fd, epoll 에 등록
				struct epoll_event	events;
				events.events	= EPOLLIN | EPOLLET;
				events.data.fd	= client_fd;

				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &events) < 0)
				{
					printf("client epoll_ctl() error\n");
					close(client_fd);
					continue;
				}
			}
			else
			{
				// epoll에 등록 된 클라이언트들의 send data 처리
				int			str_len;
				int			client_fd = epoll_events[i].data.fd;
				char		data[4096];
				str_len		= read(client_fd, &data, sizeof(data));

				if (str_len == 0)
				{
					// 클라이언트 접속 종료 요청
					printf("Client Disconnect [%d]\n", client_fd);
					close(client_fd);
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
				}
				else
				{
					// 접속 종료 요청이 아닌 경우 요청의 내용에 따라 처리.
					printf("Recv Data from [%d]\n", client_fd);
				}

			}

			
		}
	}
}