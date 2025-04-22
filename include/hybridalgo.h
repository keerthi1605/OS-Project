// #include <chrono>
// #include <cstdlib>
// #include <iostream>
// #include <ncurses.h>
// #include <signal.h>
// #include <string>
// #include <thread>
// #include <unistd.h>
// #include <vector>

// struct Process {
//   int pid;
//   int priority;
//   int burst_time;
//   int remaining_time;
// };

// struct TreeNode {
//   int priority;
//   std::vector<Process> queue;
//   TreeNode *left = nullptr;
//   TreeNode *right = nullptr;
// };

// Process create_process(int pid, int priority, int burst_time) {
//   return {pid, priority, burst_time, burst_time};
// }

// TreeNode *insert_tree_node(TreeNode *root, Process process) {
//   if (!root) {
//     root = new TreeNode{process.priority};
//     root->queue.push_back(process);
//     return root;
//   }
//   if (process.priority < root->priority)
//     root->left = insert_tree_node(root->left, process);
//   else if (process.priority > root->priority)
//     root->right = insert_tree_node(root->right, process);
//   else
//     root->queue.push_back(process);
//   return root;
// }

// bool is_tree_empty(TreeNode *root) {
//   if (!root)
//     return true;
//   if (!root->queue.empty())
//     return false;
//   return is_tree_empty(root->left) && is_tree_empty(root->right);
// }

// void in_order_traversal(TreeNode *root, int time_slice, WINDOW
// *output_window) {
//   if (!root)
//     return;

//   in_order_traversal(root->left, time_slice, output_window);

//   for (auto it = root->queue.begin(); it != root->queue.end();) {
//     Process &process = *it;
//     mvwprintw(output_window, 1, 1,
//               "Executing: Process[PID=%d, Priority=%d, Remaining Time=%d]\n",
//               process.pid, process.priority, process.remaining_time);
//     wrefresh(output_window);
//     std::this_thread::sleep_for(std::chrono::seconds(1));

//     if (process.remaining_time > time_slice) {
//       process.remaining_time -= time_slice;
//       mvwprintw(output_window, 2, 1,
//                 "Time slice completed. Remaining time: %d\n",
//                 process.remaining_time);
//       wrefresh(output_window);
//       ++it;
//     } else {
//       mvwprintw(output_window, 3, 1, "Process %d completed execution.\n",
//                 process.pid);
//       wrefresh(output_window);
//       it = root->queue.erase(it);
//     }
//   }

//   in_order_traversal(root->right, time_slice, output_window);
// }

// void hybrid_scheduling(std::vector<Process> &processes, int time_slice,
//                        WINDOW *output_window) {
//   TreeNode *root = nullptr;

//   for (const auto &process : processes) {
//     root = insert_tree_node(root, process);
//   }

//   mvwprintw(output_window, 0, 1, "Starting Scheduling...\n\n");
//   wrefresh(output_window);

//   while (!is_tree_empty(root)) {
//     in_order_traversal(root, time_slice, output_window);
//   }

//   mvwprintw(output_window, 4, 1, "Scheduling Complete.\n");
//   wrefresh(output_window);
// }

// void ncurses_display(std::vector<Process> &processes, int time_slice) {
//   initscr();
//   noecho();
//   cbreak();
//   start_color();

//   int x_max = getmaxx(stdscr);
//   WINDOW *output_window = newwin(10, x_max - 1, 0, 0);

//   init_pair(1, COLOR_GREEN, COLOR_BLACK);

//   hybrid_scheduling(processes, time_slice, output_window);

//   wrefresh(output_window);
//   std::this_thread::sleep_for(std::chrono::seconds(5));

//   endwin();
// }
