#include "ncurses_display.h"
#include "format.h"
#include "system.h"
#include <chrono>
#include <cstdlib>
#include <hybridalgo.h>
#include <iostream>
#include <signal.h>
#include <thread>
#include <unistd.h>
#include <vector>
using std::string;
using std::to_string;

std::string NCursesDisplay::ProgressBar(float percent) {
  std::string result{"0%"};
  int size{50};
  float bars{percent * size};

  for (int i{0}; i < size; ++i) {
    result += i <= bars ? '|' : ' ';
  }

  string display{to_string(percent * 100).substr(0, 4)};
  if (percent < 0.1 || percent == 1.0)
    display = " " + to_string(percent * 100).substr(0, 3);
  return result + " " + display + "/100%";
}

// System display logic
void NCursesDisplay::DisplaySystem(System &system, WINDOW *window) {
  int row{0};
  float cpuUtilization = system.Cpu().Utilization();
  int cpuPercent = static_cast<int>(cpuUtilization * 100);

  mvwprintw(window, ++row, 2, "OS: %s", system.OperatingSystem().c_str());
  mvwprintw(window, ++row, 2, "Kernel: %s", system.Kernel().c_str());
  mvwprintw(window, ++row, 2, "CPU: ");
  wattron(window, COLOR_PAIR(1));
  mvwprintw(window, row, 10, ""); // Clear previous content
  wprintw(window, ProgressBar(cpuUtilization).c_str());
  wattroff(window, COLOR_PAIR(1));

  if (cpuPercent > 80) {
    int col = (getmaxx(window) - 40) / 2;
    mvwprintw(window, ++row, col, "WARNING: CPU Utilization exceeds 80%%!");
  }

  mvwprintw(window, ++row, 2, "Memory: ");
  wattron(window, COLOR_PAIR(1));
  mvwprintw(window, row, 10, ""); // Clear previous content
  wprintw(window, ProgressBar(system.MemoryUtilization()).c_str());
  wattroff(window, COLOR_PAIR(1));
  mvwprintw(window, ++row, 2, "Total Processes: %d", system.TotalProcesses());
  mvwprintw(window, ++row, 2, "Running Processes: %d",
            system.RunningProcesses());
  mvwprintw(window, ++row, 2, "Up Time: %s",
            Format::ElapsedTime(system.UpTime()).c_str());
  wrefresh(window);
}

// Process display logic
void NCursesDisplay::DisplayProcesses(std::vector<Process> &processes,
                                      WINDOW *window, int n) {
  int row{0};
  int const pid_column{2};
  int const user_column{9};
  int const cpu_column{16};
  int const ram_column{26};
  int const time_column{35};
  int const command_column{46};
  wattron(window, COLOR_PAIR(2));
  mvwprintw(window, ++row, pid_column, "PID");
  mvwprintw(window, row, user_column, "USER");
  mvwprintw(window, row, cpu_column, "CPU[%%]");
  mvwprintw(window, row, ram_column, "RAM[MB]");
  mvwprintw(window, row, time_column, "TIME+");
  mvwprintw(window, row, command_column, "COMMAND");
  wattroff(window, COLOR_PAIR(2));
  for (int i = 0; i < n; ++i) {
    mvwprintw(window, ++row, pid_column, to_string(processes[i].Pid()).c_str());
    mvwprintw(window, row, user_column, processes[i].User().c_str());
    float cpu = processes[i].getCpuUtilization() * 100;
    mvwprintw(window, row, cpu_column, to_string(cpu).substr(0, 4).c_str());
    mvwprintw(window, row, ram_column, processes[i].Ram().c_str());
    mvwprintw(window, row, time_column,
              Format::ElapsedTime(processes[i].UpTime()).c_str());
    mvwprintw(window, row, command_column,
              processes[i].Command().substr(0, window->_maxx - 46).c_str());
  }
}

void AddProcessesInteractive(int numProcesses, bool withScheduling = false,
                             int timeQuantum = 1) {
  std::vector<pid_t> pids;
  for (int i = 0; i < numProcesses; ++i) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("Fork failed");
      exit(EXIT_FAILURE);
    } else if (pid == 0) {
      // Simulate the work
      auto start_time = std::chrono::steady_clock::now();
      std::chrono::seconds duration(15);
      while (true) {
        if (std::chrono::steady_clock::now() - start_time > duration) {
          break;
        }
      }
      exit(0); // Exit child process
    } else {
      pids.push_back(pid);
      std::cout << "Added process PID: " << pid << "\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }

  if (withScheduling) {

    while (!pids.empty()) {
      for (size_t i = 0; i < pids.size(); ++i) {
        pid_t currentProcess = pids[i];
        std::cout << "Sending SIGCONT to PID: " << currentProcess << std::endl;
        kill(currentProcess, SIGCONT);
        std::this_thread::sleep_for(std::chrono::seconds(timeQuantum));
        std::cout << "Sending SIGSTOP to PID: " << currentProcess << std::endl;
        kill(currentProcess, SIGSTOP);
      }
    }
  }
}

double MeasureCPUUtilization(System &system, int numProcesses,
                             bool withScheduling = false, int timeQuantum = 1) {
  AddProcessesInteractive(numProcesses, withScheduling, timeQuantum);
  auto start_time = std::chrono::steady_clock::now();
  while (true) {
    float cpuUtilization = system.Cpu().Utilization();
    int cpuPercent = static_cast<int>(cpuUtilization * 100);
    std::cout << "CPU Utilization: " << cpuPercent << "%\n";

    if (cpuPercent >= 80) {
      auto end_time = std::chrono::steady_clock::now();
      std::chrono::duration<double> elapsed_seconds = end_time - start_time;
      std::system("killall -9 test_process");
      return elapsed_seconds.count();
    }
    // System Overload measure
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void CompareScheduling(System &system, WINDOW *output_window) {
  int numProcesses = 10;
  int timeQuantum = 2;

  mvwprintw(output_window, 1, 1, "Measuring time without scheduling...");
  wrefresh(output_window);
  double timeWithoutScheduling =
      MeasureCPUUtilization(system, numProcesses, false);
  mvwprintw(output_window, 2, 1, "Time without scheduling: %.2f seconds",
            timeWithoutScheduling);
  wrefresh(output_window);

  mvwprintw(output_window, 3, 1, "Measuring time with scheduling...");
  wrefresh(output_window);
  double timeWithScheduling =
      MeasureCPUUtilization(system, 20, true, timeQuantum);
  mvwprintw(output_window, 4, 1, "Time with scheduling: %.2f seconds",
            timeWithScheduling);
  wrefresh(output_window);

  mvwprintw(output_window, 5, 1, "Comparison completed.");
  wrefresh(output_window);
}

void NCursesDisplay::Display(System &system, int n) {
  initscr();
  noecho();
  cbreak();
  start_color();

  int x_max{getmaxx(stdscr)};
  WINDOW *system_window = newwin(12, x_max - 1, 0, 0);
  WINDOW *process_window =
      newwin(3 + n, x_max - 1, system_window->_maxy + 1, 0);
  WINDOW *output_window =
      newwin(6, x_max - 1, system_window->_maxy + process_window->_maxy + 2, 0);

  init_pair(1, COLOR_BLUE, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);

  // CompareScheduling(system, output_window);

  while (true) {
    box(system_window, 0, 0);
    box(process_window, 0, 0);
    box(output_window, 0, 0);

    DisplaySystem(system, system_window);
    DisplayProcesses(system.Processes(), process_window, n);

    wrefresh(system_window);
    wrefresh(process_window);
    wrefresh(output_window);
    refresh();

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  endwin();
}