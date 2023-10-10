#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
void set_up_terminal() {
  if (freopen("/dev/tty", "r", stdin) == NULL) {
    fprintf(stderr, "freopen failed");
    exit(EXIT_FAILURE);
  }
  struct termios term_settings;
  if (tcgetattr(STDIN_FILENO, &term_settings) == -1) {
    fprintf(stderr, "tcgetattr failed on stdin\n");
    exit(EXIT_FAILURE);
  }
  term_settings.c_lflag &= ~ICANON;
  term_settings.c_lflag &= ~ECHO;
  term_settings.c_cc[VMIN] = 1;
  term_settings.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &term_settings) == -1) {
    fprintf(stderr, "tcsetattr_new failed on stdin\n");
    exit(EXIT_FAILURE);
  }
}
int min(int a, int b) { return a < b ? a : b; }
int max(int a, int b) { return a > b ? a : b; }
int levenshtein_dist(char *one, int one_length, char *two, int two_length) {
  int table_row_length = one_length + 1;
  int table_col_length = two_length + 1;
  int **table = malloc(table_row_length * sizeof(int *));
  for (int i = 0; i < table_row_length; ++i) {
    table[i] = malloc(table_col_length * sizeof(int));
    for (int j = 0; j < table_col_length; ++j) {
      table[i][j] = 0;
    }
  }
  table[0][0] = 0;
  for (int i = 1; i < table_row_length; ++i) {
    table[i][0] = table[i - 1][0] + 1;
  }
  for (int i = 1; i < table_col_length; ++i) {
    table[0][i] = table[0][i - 1] + 1;
  }
  for (int i = 1; i < table_row_length; ++i) {
    for (int j = 1; j < table_col_length; ++j) {
      if (one[i - 1] == two[i - 1]) {
        table[i][j] = table[i - 1][j - 1];
      } else {
        table[i][j] =
            min(table[i][j - 1], min(table[i - 1][j], table[i - 1][j - 1])) + 1;
      }
    }
  }
  int answer = table[one_length][two_length];
  for (int i = 0; i < table_row_length; ++i) {
    free(table[i]);
  }
  free(table);
  return answer;
}
void sort_on_edit_dist_output_to_sorted(
    char *sorted, char *written, int written_length, char **split_on_newlines,
    int split_on_newlines_length, int *indices, int *distances, int tab_index) {
  // insertion sort
  for (int i = 0; i < split_on_newlines_length - 1; ++i) {
    int j = indices[i + 1];
    while (distances[indices[j - 1]] > distances[indices[j]] && j > 0) {
      int temp = indices[j];
      indices[j] = indices[j - 1];
      indices[j - 1] = temp;
      --j;
    }
  }
  int currn_idx = 0;
  for (int idx = 0; idx < split_on_newlines_length; ++idx) {
    if (idx == tab_index) {
      sorted[currn_idx] = ' ';
      sorted[currn_idx + 1] = '>';
      currn_idx += 2;
    }
    strncpy(sorted + currn_idx, split_on_newlines[indices[idx]],
            strlen(split_on_newlines[indices[idx]]));
    currn_idx += strlen(split_on_newlines[indices[idx]]);
    sorted[currn_idx] = '\n';
    currn_idx++;
  }
  sorted[currn_idx] = 0;
}
void process(char **written_p, int *written_length) {
  char *written = *written_p;
  char *new_written = malloc(*written_length);
  int new_written_length = 0;
  for (int idx = 0; idx < *written_length; ++idx) {
    if (written[idx] >= 32 && written[idx] <= 127) {
      new_written[new_written_length++] = written[idx];
    }
  }
  int new_length = 0;
  for (int idx = 0; idx < new_written_length; ++idx) {
    if (new_written[idx] != 127) {
      ++new_length;
    } else {
      new_length = max(0, new_length - 1);
    }
  }
  for (int idx = 0; idx < new_length; ++idx) {
    written[idx] = new_written[idx];
  }
  free(new_written);
  *written_length = new_length;
}
char *format_str =
    "\033[0J" // Clearing everything from cursing to the end of the screen
    "\033[1K" // Erase start of line to cursor
    "\r"      // Move cursor to start of line
    "%s"      // search input
    "\0337"   // Saving cursor position
    "\n-----------------------------\n" // Separator
    "%s"                                // Entries that were piped
    "\033[%dA" // Moving cursor back to search input line
    "\0338"    // Moving cursor back to saved position
    ;
void update_sort_print(char *sorted, char **split_on_newlines,
                       int split_on_newlines_length, char *written,
                       int written_length, int *indices, int *distances,
                       int tab_index) {
  for (int i = 0; i < split_on_newlines_length; ++i) {
    indices[i] = i;
    distances[i] =
        levenshtein_dist(written, written_length, split_on_newlines[i],
                         strlen(split_on_newlines[i]));
  }
  sort_on_edit_dist_output_to_sorted(
      sorted, written, written_length, split_on_newlines,
      split_on_newlines_length, indices, distances, tab_index);
  dprintf(
      STDERR_FILENO, format_str, written, sorted, split_on_newlines_length + 2 /* adding 2 because of the ' >' and adding 1 because of the newline if tab_index is greater than 0 */);
}
void sex(char **split_on_newlines, int split_on_newlines_length) {
  set_up_terminal();
  int length_of_sorted = split_on_newlines_length + strlen(" >");
  for (int idx = 0; idx < split_on_newlines_length; ++idx) {
    length_of_sorted += strlen(split_on_newlines[idx]);
  }
  int tab_index = 0;
  int capacity = 65535;
  int length = 0;
  char *written = malloc(capacity);
  written[0] = 0;
  int *indices = malloc(split_on_newlines_length * sizeof(int));
  int *distances = malloc(split_on_newlines_length * sizeof(int));
  char *sorted = malloc(length_of_sorted);
  update_sort_print(sorted, split_on_newlines, split_on_newlines_length,
                    written, length, indices, distances, tab_index);
  while (1) {
    int r = read(STDIN_FILENO, written + length, capacity - length);
    written[r + length] = 0;
    for (int idx = length; idx < length + r;) {
      if (written[idx] == '\n' || written[idx] == '\t' ||
          written[idx] == '\v' || written[idx] == '\f') {
        if (written[idx] == '\t')
          tab_index = (tab_index + 1) % split_on_newlines_length;
        if (written[idx] == '\n') {
          fprintf(stderr, "\r");
          int current_line_in_sorted = 0;
          int prev_newline_index = 0;
          int end_of_result_idx = 0;
          for (int i = 0; i < strlen(sorted); ++i) {
            if (sorted[i] == '\n') {
              prev_newline_index = i;
              current_line_in_sorted++;
            }
            if (current_line_in_sorted == tab_index) {
              break;
            }
          }
          end_of_result_idx = prev_newline_index + 1;
          while (end_of_result_idx < strlen(sorted) &&
                 sorted[end_of_result_idx] != '\n') {
            end_of_result_idx++;
          }
          sorted[end_of_result_idx] = 0;
          dprintf(STDOUT_FILENO, "%s\n",
                  sorted + prev_newline_index + 2 + (tab_index > 0 ? 1 : 0));
          return;
        }
        for (int idx2 = idx; idx2 < length + r - 1; ++idx2) {
          written[idx2] = written[idx2 + 1];
        }
        --r;
        continue;
      }
      ++idx;
    }
    length += r;
    if (length == capacity) {
      char *temp = malloc(capacity *= 2);
      for (int idx = 0; idx < length; ++idx)
        temp[idx] = written[idx];
      free(written);
      written = temp;
    }
    process(&written, &length);
    written[length] = 0;
    update_sort_print(sorted, split_on_newlines, split_on_newlines_length,
                      written, length, indices, distances, tab_index);
  }
}
int main() {
  int input_buffer_capacity = 65535;
  int input_buffer_length = 0;
  char *input_buffer = malloc(input_buffer_capacity);
  int bytes_read;
  while ((bytes_read = read(STDIN_FILENO, input_buffer + input_buffer_length,
                            input_buffer_capacity - input_buffer_length))) {
    input_buffer_length += bytes_read;
    if (input_buffer_length == input_buffer_capacity) {
      char *temp = malloc(input_buffer_capacity *= 2);
      for (int idx = 0; idx < input_buffer_length; ++idx) {
        temp[idx] = input_buffer[idx];
      }
      free(input_buffer);
      input_buffer = temp;
    }
  }
  input_buffer[input_buffer_length] = 0;
  int newlines = 0;
  for (int idx = 0; idx < input_buffer_length; ++idx) {
    if (input_buffer[idx] == '\n')
      newlines += 1;
  }
  char **split_on_newlines = malloc(newlines * sizeof(char *));
  int last_newline_idx_plus_1 = 0;
  for (int idx = 0; idx < newlines; ++idx) {
    int next_newline_idx = last_newline_idx_plus_1 + 1;
    while (input_buffer[next_newline_idx] != '\n' &&
           next_newline_idx < input_buffer_length)
      ++next_newline_idx;
    int size = next_newline_idx - last_newline_idx_plus_1;
    split_on_newlines[idx] = malloc(size + 1);
    strncpy(split_on_newlines[idx], input_buffer + last_newline_idx_plus_1,
            size);
    split_on_newlines[idx][size] = 0;
    last_newline_idx_plus_1 = next_newline_idx + 1;
  }
  free(input_buffer);
  sex(split_on_newlines, newlines);
}