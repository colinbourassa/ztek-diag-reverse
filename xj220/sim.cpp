#include <ncurses.h>
#include <unistd.h>
#include <thread>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVPORT 5555

void readMem(WINDOW* logWindow, int clientSock)
{
  uint8_t in = 0;
  if (read(clientSock, &in, 1) == 1)
  {
    
  }
}

void clientConnectionHandler(bool& quit, WINDOW* logWindow, int clientSock)
{
  uint8_t c = 0;
  uint8_t outbyte = 0;
  while (!quit)
  {
    int bytesRead = read(clientSock, &c, 1);
    if (bytesRead == 1)
    {
      wprintw(logWindow, "%02X ", c);
      wrefresh(logWindow);

      outbyte = ~c;
      write(clientSock, &outbyte, 1);
      readMem(logWindow, clientSock);
    }
  }
}

void server(bool& quit, WINDOW* logWindow, int& servSock)
{
  wprintw(logWindow, "Starting listener...\n");
  wrefresh(logWindow);
  servSock = socket(AF_INET, SOCK_STREAM, 0);
  if (servSock >= 0)
  {
    struct sockaddr_in servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(SERVPORT);

    if (bind(servSock, (struct sockaddr*)&servAddr, sizeof(servAddr)) == 0)
    {
      if (listen(servSock, 1) == 0)
      {
        while (!quit)
        {
          wprintw(logWindow, "Server listening on %d...\n", SERVPORT);
          wrefresh(logWindow);

          socklen_t clientAddrSize = sizeof(clientAddrSize);
          int clientSock = accept(servSock, (struct sockaddr*)&servAddr, &clientAddrSize);
          if (clientSock >= 0)
          {
            wprintw(logWindow, "Client connected.\n");
            wrefresh(logWindow);
            clientConnectionHandler(quit, logWindow, clientSock);
            wprintw(logWindow, "Client disconnected.\n");
            wrefresh(logWindow);
          }
          else
          {
            wprintw(logWindow, "Failed to establish client socket.\n");
            wrefresh(logWindow);
          }
        }
      }
      else
      {
        wprintw(logWindow, "listen() failed.\n");
        wrefresh(logWindow);
      }
    }
    else
    {
      wprintw(logWindow, "bind() failed.\n");
      wrefresh(logWindow);
    }
  }
  else
  {
    wprintw(logWindow, "Failed to establish server socket.\n");
    wrefresh(logWindow);
  }

  wprintw(logWindow, "Server thread shutting down.\n");
  wrefresh(logWindow);
}

int main(void)
{
  int height = 0;
  int width = 0;
  bool quit = false;
  char cmdbuf[80];
  constexpr int cmdWindowLines = 10;

  initscr();
  getmaxyx(stdscr, height, width);
  WINDOW* logWindow = newwin(height - cmdWindowLines, width, 0, 0);
  WINDOW* cmdBorderWindow = newwin(cmdWindowLines, width, height - cmdWindowLines, 0);
  WINDOW* cmdWindow = newwin(cmdWindowLines - 2, width - 2, height - (cmdWindowLines - 1), 1);
  scrollok(logWindow, TRUE);
  scrollok(cmdWindow, TRUE);
  curs_set(0);

  box(cmdBorderWindow, 0, 0);
  wrefresh(cmdBorderWindow);

  int servSock = -1;
  std::thread serverThread(server, std::ref(quit), logWindow, std::ref(servSock));

  while (!quit)
  {
    wgetnstr(cmdWindow, cmdbuf, sizeof(cmdbuf) - 1);
    if (strcmp(cmdbuf, "q") == 0)
    {
      quit = true;
      if (servSock >= 0)
      {
        wprintw(cmdWindow, "Shutting down server socket...\n");
        wrefresh(cmdWindow);
        shutdown(servSock, SHUT_RDWR);
      }
    }
  }

  serverThread.join();
  endwin();
  return 0;
}

