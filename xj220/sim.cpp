#include <ncurses.h>
#include <unistd.h>
#include <thread>
#include <cstring>

void logThread(bool& quit, WINDOW* logWindow)
{
  int i = 0;
  while (!quit)
  {
    wprintw(logWindow, "%d - log output\n", i);
    ++i;
    sleep(1);
    wrefresh(logWindow);
  }
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

  std::thread logger(logThread, std::ref(quit), logWindow);

  while (!quit)
  {
    wgetnstr(cmdWindow, cmdbuf, sizeof(cmdbuf) - 1);
    if (strcmp(cmdbuf, "q") == 0)
    {
      wprintw(cmdWindow, "quitting!\n");
      wrefresh(cmdWindow);
      quit = true;
    }
  }

  logger.join();
  endwin();
  return 0;
}

