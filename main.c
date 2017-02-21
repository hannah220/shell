//////////////////////////////////////////
// 61419670  Hanna Yoshimochi           
//////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define TOKENNUM 50
#define TOKENSIZE 100

#define TKN_NORMAL 1
#define TKN_REDIR_IN 2
#define TKN_REDIR_OUT 3
#define TKN_PIPE 4
#define TKN_BG 5
#define TKN_EOL 6
#define TKN_EOF 7

#define NARGS 50
#define NARGV 50
#define BGPS 30


void fork_children(char **token, int *token_num, int bg);
void execute(char **token, int bc, int k);

int gettoken(char *token, int len);
int pipe_detect(char **token, int *token_num, char *argv[NARGS][NARGV]);
int in_detect(int out_pos, int in_pos, char *argv_out[NARGS][NARGV], char *argv_in[NARGS][NARGV]);
int out_detect(int com_pos, int out_pos, char *argv[NARGS][NARGV], char *argv_out[NARGS][NARGV]);
int execute_out(char *argv_out[NARGS][NARGV], int *pipefd, int num_out, int num_pipe, int s, int *pid, int fd_terminal, int bg);
void execute_out_before_pipe(char *argv_out[NARGS][NARGV], int *pipefd, int num_out, int num_pipe, int input, int out_pos, int pgid_first, int s, int *pid, int bg);
int execute_in(char *argv_in[NARGS][NARGV], int *pipefd, int in_num, int num_pipe, int out_pos, int pgid_first, int s, int is_first, int *pid, int fd_terminal, int bg);
int execute_in_next_out(char *argv_in[NARGS][NARGV], char *argv_out[NARGS][NARGV], int *pipefd, int in_num, int out_num, int out_pos, int num_pipe, int pgid_first, int s, int is_first, int *pid, int fd_terminal, int bg);
void recv_child(int sig_no);
void ign_c(int sig_no);
struct process get_bgps(int index);
int search_bgps(int pid);
void remove_bgps(int index);
void add_bgps(int pid, int ppid, int pgid);

struct process {
  int pid;
  int ppid;
  int pgid;
};

struct back_processes {
  struct process bgps[BGPS];
  int num_bgps;
};

struct back_processes bps;
void init_back_processes()
{
  int i;
  for (i = 0; i < BGPS; i++) {
    bps.bgps[i].pid = -1;
    bps.bgps[i].ppid = -1;
    bps.bgps[i].pgid = -1;
  }
  bps.num_bgps = 1;
}

struct process get_bgps(int index)
{
  return bps.bgps[index];
}

int if_bgps(int pid)
{
  int i;
  for (i = 0; i < bps.num_bgps; i++) {
    if (bps.bgps[i].pid == pid) {
      return i;
    }
  }
  return -1;
}

void add_bgps(int pid, int ppid, int pgid)
{
  struct process ps = {pid, ppid, pgid};
  bps.bgps[bps.num_bgps - 1] = ps;
  bps.num_bgps++;
}

void remove_bgps(int index)
{
  int i;
  for (i = index; i < BGPS; i++) {
    bps.bgps[i] = bps.bgps[i + 1];
  }
  bps.num_bgps--;
}

void ch_handler(int signo, siginfo_t *siginfo, void *a)
{
  int status, index;
  if ((index = if_bgps(siginfo->si_pid)) >= 0) {
    waitpid(siginfo->si_pid, &status, 0);
    remove_bgps(index);
  }
}

int main()
{
  int i, k;
  int bg = 0;
  char buf[50];
  int stop = 0;
  int end = 0;

  init_back_processes();

  ///////////////////////////////////////////////
  struct sigaction for_sigchld;
  for_sigchld.sa_sigaction = ch_handler;
  for_sigchld.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGCHLD, &for_sigchld, NULL);
  ///////////////////////////////////////////////
  signal(SIGTTOU, SIG_IGN);
  signal(SIGINT, ign_c);

  char *token[TOKENNUM];
  int token_num[TOKENNUM];
  for (;;) {
    end = 0;
    bg = 0;
    fprintf(stdout, "$ ");
    stop = 0;
    for(i = 0; stop != 1; i++) {
      token[i] = (char *)malloc(sizeof(char) * TOKENSIZE);
      token_num[i] = gettoken(token[i], strlen(token[i]));
      if (token_num[i] == 6 || token_num[i] == 7) {
	stop = 1;
      }
    }
    for (k = 0; k < i-1; k++) {
      if (k == i - 2) {
	if (strncmp(token[i - 2], "&", 1) == 0) {
	  bg = 1;
	}
      }
    }
    if (strncmp(token[0], "cd", 2) == 0) {
      if (token_num[1] != 6) {
	chdir(token[1]);
      } else {
	chdir("/Users/owner/");
      }     
      printf("\n");
      continue;
    }
    if (token_num[0] == 6) {
		//printf("\n");
      continue;
    }
    if (strncmp(token[0], "exit", 4) == 0) {
		//printf("\n");
      exit(0);
    }
    fork_children(token, token_num, bg);
    //printf("\n");    
  }
}

int gettoken(char *token, int len)
{
  char ch;
  int i = 0;
  ch = getc(stdin);
  while (ch == ' ' || ch == '\t') {
    ch = getc(stdin);
  }
  
  if (isalnum(ch) || ch == '.' || ch == '-' || ch == '*' || ch == '!' || ch == '%' || ch == '/') {
    do {
      token[i] = ch;
      i++;
      ch = getc(stdin);
    } while((isalnum(ch) && ch != '\0') || ch == '.' || ch == '-' || ch == '*' || ch == '!' || ch == '%' || ch == '/');
    if (ch == '|' || ch == '>' || ch == '&' || ch == '<' || ch == '>' || ch == '\n') {
      ch = ungetc(ch, stdin);
    }
    token[strlen(token)] = '\0';
    return TKN_NORMAL;
  } else if (ch == '|') {
    token[0] = ch;
    token[1] = '\0';
    return TKN_PIPE;
  } else if (ch == '<') {  // 2
    token[0] = ch;
    token[1] = '\0';
    return TKN_REDIR_IN;
  } else if (ch == '>') { // 3
    token[0] = ch;
    token[1] = '\0';
    return TKN_REDIR_OUT;
  } else if (ch == '&') {
    token[0] = ch;
    token[1] = '\0';
    return TKN_BG;
  } else if (ch == '\n') {
    token[0] = '\n';
    token[1] = '\0';
    return TKN_EOL;
  }
  return TKN_EOF;
}

int pid[100];
int s = -1;

void fork_children(char **token, int *token_num, int bg)
{
  
  if (token_num[0] == 7) {
	  //exit(1);
  }
  
  int i, j;
  int k = -1;
  int fw, bc;
  int file_num0, file_num1, file_num2;
  int child_pid[100];
  int child_stat[100];
  int con = 1;
  int for_break = 0;
  int com_num;
  int out_num = 0;
  int in_num = 0;
  int out_num_prev = 0;
  int num_pipe;
  int com_pos = 0;
  int out_pos = 0;
  int in_pos = 0;
  int output;
  int input;
  int status;
  int out_pos_prev;
  int in_num_prev;
  int fd_terminal;
  int pgid_first;
  int is_first;

  char *argv[NARGS][NARGV];
  for (i = 0; i < NARGS; i++) {
    for (j = 0; j < NARGV; j++) {
      argv[i][j] = (char *)malloc(sizeof(char) * 64);
    }
  }
  
  char *argv_out[NARGS][NARGV];
  for (i = 0; i < NARGS; i++) {
    for (j = 0; j < NARGV; j++) {
      argv_out[i][j] = (char *)malloc(sizeof(char) * 64);
    }
  }

  char *argv_in[NARGS][NARGV];
  for (i = 0; i < NARGS; i++) {
    for (j = 0; j < NARGV; j++) {
      argv_in[i][j] = (char *)malloc(sizeof(char) * 64);
    }
  }
  
  num_pipe = pipe_detect(token, token_num, argv);
  
  int *pipefd = (int *)malloc(sizeof(int) * num_pipe * 2);
  if (num_pipe >= 1) {
    for (i = 0; i < num_pipe; i++) {
      pipe(pipefd + 2 * i);
    }
  }

  s = -1;
  for (;con == 1;) {
    s++;
    if (num_pipe == 0) {
      errno = 0;
      com_pos = 0;
      out_num = out_detect(com_pos, out_pos, argv, argv_out);
    
      out_pos = 0;
      in_num = in_detect(out_pos, in_pos, argv_out, argv_in);

      errno = 0;

      if (out_num == 0 && in_num == 0) {	
	if ((pid[s] = fork()) == 0) {
	  com_num++;
	  if (execvp(argv[0][0], argv[0]) < 0) { 
	    if (errno == 2) {
	      fprintf(stderr, "%s: command not found\n", argv[0][0]);
	    } else {
	      perror("execvp");
	    }
	    exit(EXIT_FAILURE);
	  }
	} else {
	  setpgid(pid[s], pid[s]);
	  fd_terminal = open("/dev/tty", O_RDWR);
	  if (bg == 0) {
	    if (tcsetpgrp(fd_terminal, getpgid(pid[s])) != 0) {
	      perror("tcsetpgrp");
	      exit(EXIT_FAILURE);
	    }
	  } else if (bg == 1) {
	    add_bgps(pid[s], getpid(), getpgid(pid[s]));
	    if (tcsetpgrp(fd_terminal, getpid()) != 0) {
	      perror("tcsetpgrp");
	      exit(EXIT_FAILURE);
	    }
	  }	    
	}
      } else if (in_num >= 1 && out_num == 0) {
	pgid_first = execute_in(argv_in, pipefd, in_num, num_pipe, out_pos, pgid_first, s, is_first, pid, fd_terminal, bg);
      } else if (in_num >= 1 && out_num >= 1) {
	is_first = 1;
	pgid_first = execute_in_next_out(argv_in, argv_out, pipefd, in_num, out_num, out_pos, num_pipe, pgid_first, s, is_first, pid, fd_terminal, bg);
      } else if (in_num == 0 && out_num >= 1) {
	pgid_first = execute_out(argv_out, pipefd, out_num, num_pipe, s, pid, fd_terminal, bg);
      }
      
    } else if (num_pipe == 1) {
      errno = 0;
      com_pos = 0;
      out_num = out_detect(com_pos, out_pos, argv, argv_out);

      out_pos = 0;
      in_num = in_detect(out_pos, in_pos, argv_out, argv_in);

      if (out_num == 0 && in_num == 0) {
	if ((pid[s] = fork()) == 0) {
	  close(1);
	  dup(pipefd[1]);
	  for (j = 0; j < num_pipe * 2; j++) {
	    close(pipefd[j]);
	  }
	  com_num++;
	  if (execvp(argv[0][0], argv[0]) < 0) {	    
	    if (errno == 2) {
	      fprintf(stderr, "%s: command not found\n", argv[0][0]);
	    } else {
	      perror("execvp");
	    }
	    //exit(EXIT_FAILURE);
	  }
	  exit(0);
	} else {
	  pgid_first = pid[s];
	  setpgid(pid[s], pid[s]);
	  fd_terminal = open("/dev/tty", O_RDWR);
	  if (bg == 0) {
	    if (tcsetpgrp(fd_terminal, getpgid(pid[s])) != 0) {
	      perror("tcsetpgrp");
	      exit(EXIT_FAILURE);
	    }
	  } else if (bg == 1) {
	    add_bgps(pid[s], getpid(), getpgid(pid[s]));
	    if (tcsetpgrp(fd_terminal, getpid()) != 0) {
	      perror("tcsetpgrp");
	      exit(EXIT_FAILURE);
	    }
	  }	  
	}
      } else if (out_num >= 1 && in_num == 0) {
	pgid_first = execute_out(argv_out, pipefd, out_num, num_pipe, s, pid, fd_terminal, bg);
      } else if (in_num >= 1 && out_num == 0) {
	is_first = 1;
	pgid_first = execute_in(argv_in, pipefd, in_num, num_pipe, out_pos, pgid_first, s, is_first, pid, fd_terminal, bg);
      } else if (in_num >= 1 && out_num >= 1) {
	is_first = 1;
	pgid_first = execute_in_next_out(argv_in, argv_out, pipefd, in_num, out_num, out_pos, num_pipe, pgid_first, s, is_first, pid, fd_terminal, bg);
      }   
      
      /* ............................the second pipe .................................. */

      errno = 0;
      com_pos = 1;
      out_pos = 0;
      out_num_prev = out_num;
      out_pos += out_num_prev + 1;  
      out_num = out_detect(com_pos, out_pos, argv, argv_out);

      out_pos_prev = out_pos;
      for (k = 0; k <= out_num; k++) {
	out_pos = i + out_num_prev + k;
	in_pos = out_pos + in_num;
	in_num = in_detect(out_pos, in_pos, argv_out, argv_in);
	if (k == 0) {
	  in_num_prev = in_num;
	}
      }

      s++;
      if (out_num == 0 && in_num == 0) {
	if ((pid[s] = fork()) == 0) {
	  close(0);
	  dup(pipefd[0]);
	  for (j = 0; j < num_pipe * 2; j++) {
	    close(pipefd[j]);
	  }
	  com_num++;
	  if (execvp(argv[1][0], argv[1]) < 0) {   
	    if (errno == 2) {
	      fprintf(stderr, "%s: command not found\n", argv[1][0]);
	    } else {
	      perror("execvp");
	    }
	    exit(EXIT_FAILURE);
	  }	
	  exit(0);
	} else {
	  setpgid(pid[s], pgid_first);
	  if (bg == 1) {
	    add_bgps(pid[s], getpid(), getpgid(pid[s]));
	  }
	} 
      } else if (in_num_prev == 0 && out_num >= 1) {
	input = 0;
	execute_out_before_pipe(argv_out, pipefd, out_num, num_pipe, input, out_pos_prev, pgid_first, s, pid, bg);
      } else if (in_num_prev >= 1 && out_num == 0) {
	is_first = 0;
	pgid_first = execute_in(argv_in, pipefd, in_num_prev, num_pipe, out_pos_prev, pgid_first, s, is_first, pid, fd_terminal, bg);
      } else if (in_num_prev >= 1 && out_num >= 1) {
	is_first = 0;
	pgid_first = execute_in_next_out(argv_in, argv_out, pipefd, in_num_prev, out_num, out_pos_prev, num_pipe, pgid_first, s, is_first, pid, fd_terminal, bg);
      }
    } else if (num_pipe >= 2) {
      
      for (i = 0; i < num_pipe + 1; i++) {
	errno = 0;
	if (i == 0) {
	  input = 0;
	  output = 1;

	  com_pos = 0;
	  out_pos = 0;
	  out_num = out_detect(com_pos, out_pos, argv, argv_out);

	  out_num_prev = 0;
	  in_num = 0;
	  for (k = 0; k <= out_num; k++) {
	    out_pos = i + out_num_prev + k;
	    in_pos = out_pos + in_num;
	    in_num = in_detect(out_pos, in_pos, argv_out, argv_in);
	    if (k == 0) {
	      in_num_prev = in_num;
	    }
	  }
	  
	  if (out_num == 0 && in_num == 0) {
	    if ((pid[s] = fork()) == 0) {
	      close(1);
	      dup(pipefd[output]);
	      for (j = 0; j < num_pipe * 2; j++) {
		close(pipefd[j]);
	      }
	      execvp(argv[i][0], argv[i]);
	      if (errno == 2) {
		fprintf(stderr, "%s: command not found\n", argv[i][0]);
	      } else {
		perror("execvp");
	      }
	      exit(EXIT_FAILURE);
	    } else {
	      pgid_first = pid[s];
	      setpgid(pid[s], pid[s]);
	      fd_terminal = open("/dev/tty", O_RDWR);
	      if (bg == 0) {
		if (tcsetpgrp(fd_terminal, getpgid(pid[s])) != 0) {
		  perror("tcsetpgrp");
		  exit(EXIT_FAILURE);
		}
	      } else if (bg == 1) {
		add_bgps(pid[s], getpid(), getpgid(pid[s]));
		if (tcsetpgrp(fd_terminal, getpid()) != 0) {
		  perror("tcsetpgrp");
		  exit(EXIT_FAILURE);
		}
	      }	      
	    }
	  } else if (out_num >= 1 && in_num_prev == 0) {
	    pgid_first = execute_out(argv_out, pipefd, out_num, num_pipe, s, pid, fd_terminal, bg);
	  } else if (in_num_prev >= 1 && out_num == 0) {
	    is_first = 1;
	    out_pos_prev = 0;
	    pgid_first = execute_in(argv_in, pipefd, in_num_prev, num_pipe, out_pos_prev, pgid_first, s, is_first, pid, fd_terminal, bg);
	  } else if (in_num_prev >= 1 && out_num >= 1) {
	    is_first = 1;
	    out_pos_prev = 0;
	    pgid_first = execute_in_next_out(argv_in, argv_out, pipefd, in_num_prev, out_num, out_pos_prev, num_pipe, pgid_first, s, is_first, pid, fd_terminal, bg);
	  }
	  
	} else if (0 < i && i < num_pipe) {
	  s++;
	  is_first = 0;
	  if (i != 1) {
	    input = input + 2;
	  }
	  output = output + 2;
	  
	  out_num_prev = out_num;
	  if (i == 1) {
	    out_pos = 0;
	  }
	  out_pos += out_num_prev + 1;
	  com_pos = i;
	  out_num = out_detect(com_pos, out_pos, argv, argv_out);

	  out_pos_prev = out_pos;
	  for (k = 0; k <= out_num; k++) {
	    out_pos = i + out_num_prev + k;
	    in_pos = out_pos + in_num;
	    in_num = in_detect(out_pos, in_pos, argv_out, argv_in);
	    if (k == 0) {
	      in_num_prev = in_num;
	    }
	  }
	  
	  if (out_num == 0 && in_num == 0) {
	    if ((pid[s] = fork()) == 0) { 	    
	      close(0);
	      dup(pipefd[input]);   
	      close(1);
	      dup(pipefd[output]);
	      output = output + 2;
	      for (j = 0; j < num_pipe * 2; j++) {
		close(pipefd[j]);
	      }
	      execvp(argv[i][0], argv[i]);
	      if (errno == 2) {
		fprintf(stderr, "%s: command not found\n", argv[i][0]);
	      } else {
		perror("execvp");
	      }
	      exit(EXIT_FAILURE);
	    } else {
	      setpgid(pid[s], pgid_first);
	      if (bg == 1) {
		add_bgps(pid[s], getpid(), getpgid(pid[s]));
	      }
	    } 
	  } else if (out_num >= 1 && in_num_prev == 0) {
	    execute_out_before_pipe(argv_out, pipefd, out_num, num_pipe, input, out_pos_prev, pgid_first, s, pid, bg);
	  } else if (in_num_prev >= 1 && out_num == 0) {
	    pgid_first = execute_in(argv_in, pipefd, in_num_prev, num_pipe, out_pos_prev, pgid_first, s, is_first, pid, fd_terminal, bg);
	  } else if (in_num_prev >= 1 && out_num >= 1) {
	    pgid_first = execute_in_next_out(argv_in, argv_out, pipefd, in_num_prev, out_num, out_pos_prev, num_pipe, pgid_first, s, is_first, pid, fd_terminal, bg);
	  }
	  
	} else if (i == num_pipe) {
	  s++;
	  input = input + 2;

	  out_num_prev = out_num;
	  out_pos += out_num_prev + 1;
	  com_pos = i;
	  out_num = out_detect(com_pos, out_pos, argv, argv_out);

	  out_pos_prev = out_pos;
	  for (k = 0; k <= out_num; k++) {
	    out_pos = i + out_num_prev + k;
	    in_pos = out_pos + in_num;
	    in_num = in_detect(out_pos, in_pos, argv_out, argv_in);
	  }

	  if (out_num == 0 && in_num == 0) {
	    if ((pid[s] = fork()) == 0) {  
	      close(0);
	      dup(pipefd[input]);
	      for (j = 0; j < num_pipe * 2; j++) {
		close(pipefd[j]);
	      }
	      execvp(argv[i][0], argv[i]);
	      if (errno == 2) {
		fprintf(stderr, "%s: command not found\n", argv[i][0]);
	      } else {
		perror("execvp");
	      }
	      exit(EXIT_FAILURE);
	    } else {
	      setpgid(pid[s], pgid_first);
	      if (bg == 1) {
		add_bgps(pid[s], getpid(), getpgid(pid[s]));
	      }
	    }
	  } else if (out_num >= 1 && in_num == 0) {
	    execute_out_before_pipe(argv_out, pipefd, out_num, num_pipe, input, out_pos_prev, pgid_first, s, pid, bg);
	  } else if (in_num >= 1 && out_num == 0) {
	    pgid_first = execute_in(argv_in, pipefd, in_num, num_pipe, out_pos_prev, pgid_first, s, is_first, pid, fd_terminal, bg);
	  } else if (in_num >= 1 && out_num >= 1) {
	    pgid_first = execute_in_next_out(argv_in, argv_out, pipefd, in_num, out_num, out_pos_prev, num_pipe, pgid_first, s, is_first, pid, fd_terminal, bg);
	  }	  
	}	
      }
    }  
    con = 0;
  }
  
  for (i = 0; i < num_pipe * 2; i++) {
    close(pipefd[i]);
  }
  if (bg == 0) {
    for (i = 0; i < num_pipe + 1; i++) {
      child_pid[i] = wait(&child_stat[i]);
      //printf("no.%d child %d was finished\n ", i, child_pid[i]);
    }
    tcsetpgrp(fd_terminal, getpid());
    close(fd_terminal);
  } 
}    

int pipe_detect(char **token, int *token_num, char *argv[NARGS][NARGV]) {
  int i, j;
  int arg_pos, com_pos;
  int k = 0;
  int tokens;
  int com_num = 0;
  int num_pipe = 0;
   
  while (token_num[k] != TKN_EOL) {
    k++;
  }
  if (token_num[k - 1] == TKN_BG) {
    k--;
  }

  com_pos = 0;
  arg_pos = 0;
  
  tokens = k;
  for (i = 0; i < tokens; i++) {
    if (token_num[i] != TKN_PIPE) {
      strncpy(argv[com_pos][arg_pos], token[i], 10);
      arg_pos++;
    } else if (token_num[i] == TKN_PIPE) {
      argv[com_pos][arg_pos] = NULL;
      com_pos++;
      arg_pos = 0;
      com_num++;
      num_pipe++;
    } 
  }
  argv[com_pos][arg_pos] = NULL;
  return num_pipe;
}

int out_detect(int com_pos, int out_pos, char *argv[NARGS][NARGV], char *argv_out[NARGS][NARGV]) {
  int i;
  int num_out = 0;
  int arg_pos = 0;
  
  for (i = 0; argv[com_pos][i] != NULL; i++) {
    if (strncmp(argv[com_pos][i], ">", 1) == 0) {
      num_out++;
    }
  }
  for (i = 0; argv[com_pos][i] != NULL; i++) {
    if (strncmp(argv[com_pos][i], ">", 1) != 0) {
      strncpy(argv_out[out_pos][arg_pos], argv[com_pos][i], 30);
      arg_pos++;
    } else if (strncmp(argv[com_pos][i], ">", 1) == 0) {
      argv_out[out_pos][arg_pos] = NULL;
      out_pos++;
      arg_pos = 0;
    }
  }
  argv_out[out_pos][arg_pos] = NULL;
  return num_out;
}

int in_detect(int out_pos, int in_pos, char *argv_out[NARGS][NARGV], char *argv_in[NARGS][NARGV]) {
  int i;
  int num_in = 0;
  int arg_pos = 0;
  
  for (i = 0; argv_out[out_pos][i] != NULL; i++) {
    if (strncmp(argv_out[out_pos][i], "<", 1) == 0) {
      num_in++;
    }
  }
  if (num_in == 0) {
    return 0;
  }
  for (i = 0; argv_out[out_pos][i] != NULL; i++) {
    if (strncmp(argv_out[out_pos][i], "<", 1) != 0) {
      strncpy(argv_in[in_pos][arg_pos], argv_out[out_pos][i], 10);
      arg_pos++;
    } else if (strncmp(argv_out[out_pos][i], "<", 1) == 0) {
      argv_in[in_pos][arg_pos] = NULL;
      in_pos++;
      arg_pos = 0;
    }
  }
  argv_in[in_pos][arg_pos] = NULL;
  return num_in;
}

int execute_out(char *argv_out[NARGS][NARGV], int *pipefd, int num_out, int num_pipe, int s, int *pid, int fd_terminal, int bg) {
  errno = 0;
  int i, j;
  int pgid_first;
  int file_num0;
  if (num_out == 1) {
    if ((pid[s] = fork()) == 0) {
      close(1);
      file_num0 = open(argv_out[1][0], O_WRONLY|O_CREAT|O_TRUNC, 0644);
      dup(file_num0);
      for (j = 0; j < num_pipe * 2; j++) {
	close(pipefd[j]);
      }
      if (execvp(argv_out[0][0], argv_out[0]) < 0) {
	perror("execvp");
	if (errno == 2) {
	  fprintf(stderr, "%s: command not found\n", argv_out[0][0]);
	}
	exit(EXIT_FAILURE);
      }
    } else {
      pgid_first = pid[s];
      setpgid(pid[s], pid[s]);
      fd_terminal = open("/dev/tty", O_RDWR);
      if (bg == 0) {
	if (tcsetpgrp(fd_terminal, getpgid(pid[s])) != 0) {
	  perror("tcsetpgrp");
	  exit(EXIT_FAILURE);
	}
      } else if (bg == 1) {
	add_bgps(pid[s], getpid(), getpgid(pid[s]));
	if (tcsetpgrp(fd_terminal, getpid()) != 0) {
	  perror("tcsetpgrp");
	  exit(EXIT_FAILURE);
	}
      }      
    }
  } else if (num_out >= 1) {
    if ((pid[s] = fork()) == 0) {
      close(1);
      file_num0 = open(argv_out[num_out][0], O_WRONLY|O_CREAT|O_TRUNC, 0644);
      dup(file_num0);
      for (i = 0; i < num_out - 1; i++) {
	open(argv_out[i + 1][0], O_WRONLY|O_CREAT|O_TRUNC, 0644);
      }
      for (j = 0; j < num_pipe * 2; j++) {
	close(pipefd[j]);
      }
      if (execvp(argv_out[0][0], argv_out[0]) < 0) {
	perror("execvp");
	if (errno == 2) {
	  fprintf(stderr, "%s: command not found\n", argv_out[0][0]);
	}
	exit(EXIT_FAILURE);
      }
    } else {
      pgid_first = pid[s];
      setpgid(pid[s], pid[s]);
      fd_terminal = open("/dev/tty", O_RDWR);
      if (bg == 0) {
	if (tcsetpgrp(fd_terminal, getpgid(pid[s])) != 0) {
	  perror("tcsetpgrp");
	  exit(EXIT_FAILURE);
	}
      } else if (bg == 1) {
	add_bgps(pid[s], getpid(), getpgid(pid[s]));
	if (tcsetpgrp(fd_terminal, getpid()) != 0) {
	  perror("tcsetpgrp");
	  exit(EXIT_FAILURE);
	}
      }  
    }
  }
  close(file_num0);
  return pgid_first;
}

void execute_out_before_pipe(char *argv_out[NARGS][NARGV], int *pipefd, int num_out, int num_pipe, int input, int out_pos, int pgid_first, int s, int *pid, int bg) {
  int i, j;
  int file_num0;
  errno = 0;
  if (num_out == 1) {
    if ((pid[s] = fork()) == 0) {
      close(0);
      dup(pipefd[input]);
      close(1);
      file_num0 = open(argv_out[out_pos + 1][0], O_WRONLY|O_CREAT|O_TRUNC, 0644);
      dup(file_num0);
      for (j = 0; j < num_pipe * 2; j++) {
	close(pipefd[j]);
      }
      if (execvp(argv_out[out_pos][0], argv_out[out_pos]) < 0) {
	perror("execvp");
	if (errno == 2) {
	  fprintf(stderr, "%s: command not found\n", argv_out[out_pos][0]);
	}
	exit(EXIT_FAILURE);
      }
    } else {
      setpgid(pid[s], pgid_first);
      if (bg == 1) {
	add_bgps(pid[s], getpid(), getpgid(pid[s]));
      }
    }
  } else if (num_out >= 1) {
    if ((pid[s] = fork()) == 0) {
      close(0);
      dup(pipefd[input]);
      close(1);
      file_num0 = open(argv_out[out_pos + num_out + 1][0], O_WRONLY|O_CREAT|O_TRUNC, 0644);
      dup(file_num0);
      for (i = 0; i < num_out - 1; i++) {
	open(argv_out[out_pos + i + 1][0], O_WRONLY|O_CREAT|O_TRUNC, 0644);
      }
      for (j = 0; j < num_pipe * 2; j++) {
	close(pipefd[j]);
      }
      if (execvp(argv_out[out_pos][0], argv_out[out_pos]) < 0) {
	perror("execvp");
	if (errno == 2) {
	  fprintf(stderr, "%s: command not found\n", argv_out[out_pos][0]);
	}
	exit(EXIT_FAILURE);
      }
    } else {
      setpgid(pid[s], pgid_first);
      if (bg == 1) {
	add_bgps(pid[s], getpid(), getpgid(pid[s]));
      }
    }
  }
  close(file_num0);
}

int execute_in(char *argv_in[NARGS][NARGV], int *pipefd, int in_num, int num_pipe, int out_pos, int pgid_first, int s, int is_first, int *pid, int fd_terminal, int bg) {
  int j;
  int file_num0;
  errno = 0;
  if (in_num == 1) {
    if ((pid[s] = fork()) == 0) {
      close(0);
      file_num0 = open(argv_in[out_pos + 1][0], O_RDONLY, 0644);     
      dup(file_num0);
      for (j = 0; j < num_pipe * 2; j++) {
	close(pipefd[j]);
      }
      if (execvp(argv_in[out_pos][0], argv_in[out_pos]) < 0) {
	perror("execvp");
	if (errno == 2) {
	  fprintf(stderr, "%s: command not found\n", argv_in[out_pos][0]);
	}
	exit(EXIT_FAILURE);
      }
    } else {
      if (is_first == 1) {
	pgid_first = pid[s];
	setpgid(pid[s], pid[s]);
	fd_terminal = open("/dev/tty", O_RDWR);
	if (bg == 0) {
	  if (tcsetpgrp(fd_terminal, getpgid(pid[s])) != 0) {
	    perror("tcsetpgrp");
	    exit(EXIT_FAILURE);
	  }
	} else if (bg == 1) {
	  add_bgps(pid[s], getpid(), getpgid(pid[s]));
	  if (tcsetpgrp(fd_terminal, getpid()) != 0) {
	    perror("tcsetpgrp");
	    exit(EXIT_FAILURE);
	  }
	}
	
      } else {
	setpgid(pid[s], pgid_first);
	if (bg == 1) {
	  add_bgps(pid[s], getpid(), getpgid(pid[s]));
	}
      }
    }
  } else if (in_num >= 1) {
    if ((pid[s] = fork()) == 0) {
      close(0);
      file_num0 = open(argv_in[out_pos + in_num][0], O_RDONLY, 0644);     
      dup(file_num0);
      for (j = 0; j < num_pipe * 2; j++) {
	close(pipefd[j]);
      }
      if (execvp(argv_in[out_pos][0], argv_in[out_pos]) < 0) {
	perror("execvp");
	if (errno == 2) {
	  fprintf(stderr, "%s: command not found\n", argv_in[out_pos][0]);
	}
	exit(EXIT_FAILURE);
      }
    } else {
      if (is_first == 1) {
	pgid_first = pid[s];
	setpgid(pid[s], pid[s]);
	fd_terminal = open("/dev/tty", O_RDWR);
	if (bg == 0) {
	  if (tcsetpgrp(fd_terminal, getpgid(pid[s])) != 0) {
	    perror("tcsetpgrp");
	    exit(EXIT_FAILURE);
	  }
	} else if (bg == 1) {
	  add_bgps(pid[s], getpid(), getpgid(pid[s]));
	  if (tcsetpgrp(fd_terminal, getpid()) != 0) {
	    perror("tcsetpgrp");
	    exit(EXIT_FAILURE);
	  }
	}
      } else {
	setpgid(pid[s], pgid_first);
	if (bg == 1) {
	  add_bgps(pid[s], getpid(), getpgid(pid[s]));
	}
      }
    }
  }
  close(file_num0);
  return pgid_first;
}

int execute_in_next_out(char *argv_in[NARGS][NARGV], char *argv_out[NARGS][NARGV], int *pipefd, int in_num, int out_num, int out_pos, int num_pipe, int pgid_first, int s, int is_first, int *pid, int fd_terminal, int bg) {
  int j;
  int file_num0;
  int file_num1;
  errno = 0;
  if ((pid[s] = fork()) == 0) {
    close(0);
    file_num0 = open(argv_in[out_pos + in_num][0], O_RDONLY, 0644);     
    dup(file_num0);
    close(1);
    file_num1 = open(argv_out[out_pos + out_num][0], O_WRONLY|O_CREAT|O_TRUNC, 0644);
    dup(file_num1);
    for (j = 0; j < num_pipe * 2; j++) {
      close(pipefd[j]);
    }
    if (execvp(argv_in[out_pos][0], argv_in[out_pos]) < 0) {
      perror("execvp");
      if (errno == 2) {
	fprintf(stderr, "%s: command not found\n", argv_in[out_pos][0]);
      }
      exit(EXIT_FAILURE);
    }
  } else {
    if (is_first == 1) {
      pgid_first = pid[s];
      setpgid(pid[s], pid[s]);
      fd_terminal = open("/dev/tty", O_RDWR);
      if (bg == 0) {
	if (tcsetpgrp(fd_terminal, getpgid(pid[s])) != 0) {
	  perror("tcsetpgrp");
	  exit(EXIT_FAILURE);
	}
      } else if (bg == 1) {
	add_bgps(pid[s], getpid(), getpgid(pid[s]));
	if (tcsetpgrp(fd_terminal, getpid()) != 0) {
	  perror("tcsetpgrp");
	  exit(EXIT_FAILURE);
	}
      }     
    } else {
      setpgid(pid[s], pgid_first);
      if (bg == 1) {
	add_bgps(pid[s], getpid(), getpgid(pid[s]));
      }
    }
  }
  close(file_num0);
  close(file_num1);
  return pgid_first;
}

void ign_c(int sig_no) {
  signal(sig_no, SIG_IGN);
  fprintf(stderr, "\n$ ");
}
