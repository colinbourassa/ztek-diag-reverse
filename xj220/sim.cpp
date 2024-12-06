#include <cstdlib>
#include <ncurses.h>
#include <unistd.h>
#include <thread>
#include <map>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVPORT 5555

enum class RState
{
  Cmd,
  WriteAddrHi,
  WriteAddrLo,
  WriteValue,
  ReadAddrHi,
  ReadAddrLo,
  ReadNumBytesHi,
  ReadNumBytesLo
};

void echo(int clientSock, uint8_t byte, WINDOW* logWindow)
{
  // Echoes are the two's-complement of the byte being echoed
  const uint8_t inverted = ~byte + 1;
  wprintw(logWindow, "<%02X> ", inverted);
  wrefresh(logWindow);
  write(clientSock, &inverted, 1);
}

void clientConnectionHandler(bool& quit,
                             WINDOW* logWindow,
                             int clientSock,
                             const std::vector<uint8_t>& dataFrame,
                             std::map<uint16_t,uint8_t>& memory)
{
  RState state = RState::Cmd;
  uint8_t in = 0;
  uint8_t out = 0;
  uint16_t address = 0;
  uint16_t bytecount = 0;
  uint8_t checksum = 0;

  while (!quit)
  {
    if (read(clientSock, &in, 1) == 1)
    {
      if (state == RState::Cmd)
      {
        const auto duration = std::chrono::system_clock::now().time_since_epoch();
        const double secs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;
        wprintw(logWindow, "\n[%.3f] ", secs);
      }
      wprintw(logWindow, "%02X ", in);
      wrefresh(logWindow);

      switch (state)
      {
      case RState::Cmd:
        switch (in)
        {
        case 0x02:
          echo(clientSock, in, logWindow);
          checksum = 0;
          wprintw(logWindow, "\nSending data frame of %d bytes", dataFrame.size());
          wrefresh(logWindow);
          for (auto dataFrameByte : dataFrame)
          {
            checksum += dataFrameByte;
            out = ~dataFrameByte + 1;
            write(clientSock, &out, 1);
          }
          out = ~(0xff - checksum) + 1;
          write(clientSock, &out, 1);
          break;
        case 0x03:
          state = RState::ReadAddrHi;
          echo(clientSock, in, logWindow);
          break;
        case 0x08:
          state = RState::WriteAddrHi;
          echo(clientSock, in, logWindow);
          break;
        default:
          wprintw(logWindow, "Cmd byte unknown.");
          wrefresh(logWindow);
        }
        break;

      case RState::ReadAddrHi:
        address = static_cast<uint16_t>(in) << 8;
        state = RState::ReadAddrLo;
        echo(clientSock, in, logWindow);
        break;

      case RState::ReadAddrLo:
        address |= in;
        state = RState::ReadNumBytesHi;
        echo(clientSock, in, logWindow);
        break;

      case RState::ReadNumBytesHi:
        bytecount = static_cast<uint16_t>(in) << 8;
        state = RState::ReadNumBytesLo;
        echo(clientSock, in, logWindow);
        break;

      case RState::ReadNumBytesLo:
        bytecount |= in;
        echo(clientSock, in, logWindow);
        wprintw(logWindow, "\nReading %d byte(s) starting at offset %04X.\n", bytecount, address);
        wrefresh(logWindow);
        checksum = 0;
        for (uint16_t cAddr = address; cAddr < (address + bytecount); cAddr++)
        {
          checksum += memory[cAddr];
          out = ~(memory[cAddr] - 1);
          wprintw(logWindow, "<%02X> ", out);
          wrefresh(logWindow);
          write(clientSock, &out, 1);
        }

        // Newer revisions of the ECU firmware send a checksum at the end
        // of the memory read block.
        if (memory[0xFFDE] >= 0x36)
        {
          out = ~(0xff - checksum) + 1;
          wprintw(logWindow, "{%02X} ", out);
          wrefresh(logWindow);
          write(clientSock, &out, 1);
        }

        state = RState::Cmd;
        break;

      case RState::WriteAddrHi:
        address = static_cast<uint16_t>(in) << 8;
        state = RState::WriteAddrLo;
        echo(clientSock, in, logWindow);
        break;

      case RState::WriteAddrLo:
        address |= in;
        state = RState::WriteValue;
        echo(clientSock, in, logWindow);
        break;

      case RState::WriteValue:
        echo(clientSock, in, logWindow);
        out = 0x0F;
        write(clientSock, &out, 1);
        memory[address] = in;
        state = RState::Cmd;
        break;
      }
    }
  }
}

void server(bool& quit,
            WINDOW* logWindow,
            int& servSock,
            const std::vector<uint8_t>& dataFrame,
            std::map<uint16_t,uint8_t>& memory)
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
            clientConnectionHandler(quit, logWindow, clientSock, dataFrame, memory);
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

  std::vector<uint8_t> dataFrame;
  dataFrame.resize(68);
  std::map<uint16_t,uint8_t> memoryContent = { { 0xFFDE, 0x36 } };

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
  std::thread serverThread(server,
                           std::ref(quit),
                           logWindow,
                           std::ref(servSock),
                           std::ref(dataFrame),
                           std::ref(memoryContent));
  wprintw(logWindow, "Data frame contains %d bytes.\n", dataFrame.size());
  wrefresh(logWindow);

  while (!quit)
  {
    wgetnstr(cmdWindow, cmdbuf, sizeof(cmdbuf) - 1);
    if (cmdbuf[0] == 'q')
    {
      quit = true;
      if (servSock >= 0)
      {
        wprintw(cmdWindow, "Shutting down server socket...\n");
        wrefresh(cmdWindow);
        shutdown(servSock, SHUT_RDWR);
      }
    }
    else if (cmdbuf[0] == 'd')
    {
      char* p = strtok(&cmdbuf[2], " ");
      uint16_t offset = strtoul(p, NULL, 16);
      p = strtok(NULL, " ");
      uint8_t value = strtoul(p, NULL, 0);
      dataFrame[offset] = value;
      wprintw(cmdWindow, "Set data frame byte %04X to %02X\n", offset, value);
    }
    else if (cmdbuf[0] == 'm')
    {
      char* p = strtok(&cmdbuf[2], " ");
      uint16_t offset = strtoul(p, NULL, 16);
      p = strtok(NULL, " ");
      uint8_t value = strtoul(p, NULL, 0);
      memoryContent[offset] = value;
      wprintw(cmdWindow, "Set memory byte at %04X to %02X\n", offset, value);
    }
  }

  serverThread.join();
  endwin();
  return 0;
}

