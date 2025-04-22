#include "ncurses_display.h"
#include "format.h"
#include "system.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <hybridalgo.h>
#include <iostream>
#include <queue>
#include <signal.h>
#include <thread>
#include <unistd.h>
#include <vector>
using std::string;
using std::to_string;

struct GanttData {
  int pid;
  int burst;
  int start;
  int completion;
};

struct SimulatedProcess {
  int pid;
  int arrivalTime;
  int burstTime;
  int remainingTime;
  std::string status = "Waiting";
};

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

std::string SimProgressBar(float percent) {
  std::string result{"0%"};
  int size{50};
  float bars{percent * size};

  for (int i{0}; i < size; ++i)
    result += i <= bars ? '|' : ' ';
  string display = to_string(percent * 100).substr(0, 4);
  if (percent < 0.1 || percent == 1.0)
    display = " " + to_string(percent * 100).substr(0, 3);
  return result + " " + display + "/100%";
}

void DisplaySimSystem(float cpuUtil, int totalProc, int runProc,
                      int elapsedTime, WINDOW *win) {
  int row = 1;
  mvwprintw(win, row++, 2, "Simulated CPU:");
  wattron(win, COLOR_PAIR(1));
  mvwprintw(win, row++, 2, SimProgressBar(cpuUtil).c_str());
  wattroff(win, COLOR_PAIR(1));
  mvwprintw(win, row++, 2, "Total Processes: %d", totalProc);
  mvwprintw(win, row++, 2, "Running: %d", runProc);
  mvwprintw(win, row++, 2, "Sim Time: %d sec", elapsedTime);
  wrefresh(win);
}

void DisplaySimProcesses(std::vector<SimulatedProcess> &proc, WINDOW *win) {
  int row = 1;
  wattron(win, COLOR_PAIR(2));
  mvwprintw(win, row++, 2, "PID  ARR  BUR  REM  STATUS");
  wattroff(win, COLOR_PAIR(2));
  for (auto &p : proc) {
    mvwprintw(win, row++, 2, "%4d  %3d  %3d  %3d  %s", p.pid, p.arrivalTime,
              p.burstTime, p.remainingTime, p.status.c_str());
  }
  wrefresh(win);
}

void SimulateScheduling(WINDOW *sysWin, WINDOW *procWin, WINDOW *outWin,
                        int timeQuantum = 2) {
  int time = 0, pidCounter = 1000;
  std::vector<SimulatedProcess> allProcesses = {{pidCounter++, 0, 5, 5},
                                                {pidCounter++, 1, 4, 4},
                                                {pidCounter++, 2, 6, 6},
                                                {pidCounter++, 3, 3, 3},
                                                {pidCounter++, 4, 2, 2}};

  std::queue<int> readyQueue;
  std::vector<bool> arrived(allProcesses.size(), false);
  int active = -1;
  int quantumLeft = 0;

  while (true) {
    for (int i = 0; i < allProcesses.size(); ++i) {
      if (!arrived[i] && allProcesses[i].arrivalTime <= time) {
        readyQueue.push(i);
        allProcesses[i].status = "Ready";
        arrived[i] = true;
      }
    }

    if ((active == -1 || quantumLeft == 0) && !readyQueue.empty()) {
      if (active != -1 && allProcesses[active].remainingTime > 0) {
        allProcesses[active].status = "Ready";
        readyQueue.push(active);
      }
      active = readyQueue.front();
      readyQueue.pop();
      allProcesses[active].status = "Running";
      quantumLeft = timeQuantum;
    }

    if (active != -1) {
      allProcesses[active].remainingTime--;
      quantumLeft--;
      if (allProcesses[active].remainingTime == 0) {
        allProcesses[active].status = "Done";
        active = -1;
      }
    }

    float cpuUtil = active != -1 ? 0.9 : 0.3;

    box(sysWin, 0, 0);
    box(procWin, 0, 0);
    box(outWin, 0, 0);
    DisplaySimSystem(cpuUtil, allProcesses.size(), active != -1 ? 1 : 0, time,
                     sysWin);
    DisplaySimProcesses(allProcesses, procWin);
    mvwprintw(outWin, 1, 2, "Round Robin (TQ = %d) Simulation Running...",
              timeQuantum);
    wrefresh(outWin);

    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    time++;

    bool allDone = std::all_of(allProcesses.begin(), allProcesses.end(),
                               [](auto &p) { return p.remainingTime == 0; });
    if (allDone)
      break;
  }

  mvwprintw(outWin, 3, 2, "Simulation Complete. Press any key to exit.");
  wrefresh(outWin);
  getch();
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

void NCursesDisplay::DisplaySystem(System &system, WINDOW *window) {
  int row{0};
  float cpuUtilization = system.Cpu().Utilization();
  int cpuPercent = static_cast<int>(cpuUtilization * 100);

  mvwprintw(window, ++row, 2, "OS: %s", system.OperatingSystem().c_str());
  mvwprintw(window, ++row, 2, "Kernel: %s", system.Kernel().c_str());
  mvwprintw(window, ++row, 2, "CPU: ");
  wattron(window, COLOR_PAIR(1));
  mvwprintw(window, row, 10, "");
  wprintw(window, ProgressBar(cpuUtilization).c_str());
  wattroff(window, COLOR_PAIR(1));

  if (cpuPercent > 80) {
    int col = (getmaxx(window) - 40) / 2;
    mvwprintw(window, ++row, col, "WARNING: CPU Utilization exceeds 80%%!");
  }

  mvwprintw(window, ++row, 2, "Memory: ");
  wattron(window, COLOR_PAIR(1));
  mvwprintw(window, row, 10, "");
  wprintw(window, ProgressBar(system.MemoryUtilization()).c_str());
  wattroff(window, COLOR_PAIR(1));
  mvwprintw(window, ++row, 2, "Total Processes: %d", system.TotalProcesses());
  mvwprintw(window, ++row, 2, "Running Processes: %d",
            system.RunningProcesses());
  mvwprintw(window, ++row, 2, "Up Time: %s",
            Format::ElapsedTime(system.UpTime()).c_str());
  wrefresh(window);
}

void NCursesDisplay::DisplayProcesses(std::vector<Process> &processes,
                                      WINDOW *window, int n) {
  int row{0};
  
  int const pid_column{2};      
  int const arr_column{9};      
  int const bur_column{16};     
  int const rem_column{25};     
  int const stat_column{34};   
  int const user_column{42};    
  int const cpu_column{50};     
  int const ram_column{60};     
  int const time_column{70};    
  int const command_column{80}; 

  wattron(window, COLOR_PAIR(2));
  
  mvwprintw(window, ++row, pid_column, "PID");
  mvwprintw(window, row, arr_column, "ARR");
  mvwprintw(window, row, bur_column, "BUR");
  mvwprintw(window, row, rem_column, "REM");
  mvwprintw(window, row, stat_column, "STAT");
  mvwprintw(window, row, user_column, "USER");
  mvwprintw(window, row, cpu_column, "CPU%%");
  mvwprintw(window, row, ram_column, "RAM");
  mvwprintw(window, row, time_column, "TIME+");
  mvwprintw(window, row, command_column, "COMMAND");
  wattroff(window, COLOR_PAIR(2));

  for (int i = 0; i < n && i < static_cast<int>(processes.size()); ++i) {
    mvwprintw(window, ++row, pid_column, "%d", processes[i].Pid());
    mvwprintw(window, row, arr_column, "%d", processes[i].ArrivalTime());
    mvwprintw(window, row, bur_column, "%d", processes[i].BurstTime());
    mvwprintw(window, row, rem_column, "%d", processes[i].RemainingTime());
    mvwprintw(window, row, stat_column, "%s", processes[i].Status().c_str());
    mvwprintw(window, row, user_column, "%s", processes[i].User().c_str());

    float cpu = processes[i].getCpuUtilization() * 100;
    mvwprintw(window, row, cpu_column, "%.1f", cpu);

    mvwprintw(window, row, ram_column, "%s", processes[i].Ram().c_str());
    mvwprintw(window, row, time_column, "%s",
              Format::ElapsedTime(processes[i].UpTime()).c_str());
    mvwprintw(window, row, command_column, "%.40s",
              processes[i].Command().c_str());
  }
  wrefresh(window);
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
  WINDOW *sim_sys_win =
      newwin(7, x_max - 2, system_window->_maxy + process_window->_maxy + 2, 1);
  WINDOW *sim_proc_win = newwin(
      10, x_max - 2, system_window->_maxy + process_window->_maxy + 9, 1);
  WINDOW *sim_out_win = newwin(
      5, x_max - 2, system_window->_maxy + process_window->_maxy + 19, 1);

  init_pair(1, COLOR_BLUE, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);

  nodelay(stdscr, TRUE);
  CompareScheduling(system, sim_out_win);
  while (true) {
    box(system_window, 0, 0);
    box(process_window, 0, 0);
    box(sim_sys_win, 0, 0);
    box(sim_proc_win, 0, 0);
    box(sim_out_win, 0, 0);

    DisplaySystem(system, system_window);
    DisplayProcesses(system.Processes(), process_window, n);

    int ch = getch();
    if (ch == 'S' || ch == 's') {
      SimulateScheduling(sim_sys_win, sim_proc_win, sim_out_win);
      werase(sim_sys_win);
      werase(sim_proc_win);
      werase(sim_out_win);
      wrefresh(sim_sys_win);
      wrefresh(sim_proc_win);
      wrefresh(sim_out_win);

      wrefresh(system_window);
      wrefresh(process_window);
      wrefresh(sim_sys_win);
      wrefresh(sim_proc_win);
      wrefresh(sim_out_win);
      refresh();
    }

    wrefresh(system_window);
    wrefresh(process_window);
    wrefresh(sim_sys_win);
    wrefresh(sim_proc_win);
    wrefresh(sim_out_win);
    refresh();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  endwin();
}
