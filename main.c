/******************************************************************************
 *
 *  File Name........: main.c
 *
 *  Description......: Simple driver program for ush's parser
 *
 *  Author...........: Vincent W. Freeh
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "parse.h"
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#define NO_FILE 0

int fd;
int mypipe[2][2];
int flag;
char host[20];
extern char **environ;
static char* is_exists_cmd(Cmd command);
static void open_pipe(int pipe[]);
static void do_presets(Cmd command);
static void setup(Cmd command, int *input_file);
static void quit_handler(int signum);
static void int_handler(int signum);
static void term_handler(int signum);
static void main_handler(Cmd command);
static char* get_flag();
static void set_flag(int a);
static void flag_switch();

static void set_flag(int a) {
  if (a == 0 || a == 1)
  {
    flag = a;
  }
}

static char* get_flag() {
  if (flag == 1)
    return "True";
  else
    return "False";
}

static void quit_handler(int signum) {
  signal(signum, SIG_IGN);
  printf("\n%s%% ", host);
  fflush(STDIN_FILENO);
  signal(signum, quit_handler);
}

static void int_handler(int signum) {
  printf("\r\n");
  printf("%s%% ", host);
  fflush(STDIN_FILENO);
}

static void term_handler(int signum)
{
  signal(signum, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  killpg(getpgrp(), SIGTERM);
  signal(signum, term_handler);
  exit(0);
}




static void setup(Cmd command, int *input_file) {
//__________________Command In handling_________________//

  if (command->in == Tin)
  {
    *input_file = open(command->infile, O_RDONLY, 0660);
    dup2(*input_file, 0);
  }
  else if (command->in == Tpipe)
  {

    if (strcmp(get_flag(), "False") == 0)
    {
      close(mypipe[0][1]);
      dup2(mypipe[0][0], 0);
    }
    else
    {
      close(mypipe[1][1]);
      dup2(mypipe[1][0], 0);
    }
  }

  else if (command->in == TpipeErr)
  {

    if (strcmp(get_flag(), "False") == 0)
    {
      close(mypipe[0][1]);
      dup2(mypipe[0][0], 0);
    }
    else
    {
      close(mypipe[1][1]);
      dup2(mypipe[1][0], 0);
    }
  }
  /*__________End Command In handling_______________*/

  if (command->out == ToutErr)
  {
    dup2(1, 2);
    fflush(stdout);
    fd = creat(command->outfile, 0660);
    dup2(fd, 1);
  }
  else if (command->out == TappErr)
  {
    dup2(1, 2);
    fflush(stdout);
    fd = open(command->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
    dup2(fd, 1);
  }
  else if (command->out == Tout)
  {
    fflush(stdout);
    fd = creat(command->outfile, 0660);
    dup2(fd, 1);
  }
  else if (command->out == Tapp)
  {
    fflush(stdout);
    fd = open(command->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
    dup2(fd, 1);
  }
  else if (command->out == Tpipe || command->out == TpipeErr)
  {

    //_________________________________________________//

    if (command->next != NULL && command->next->in == Tpipe)
    {
      if (strcmp(get_flag(), "True") == 0)
      {
        close(mypipe[0][0]);
        dup2(mypipe[0][1], 1);
      }
      else
      {
        close(mypipe[1][0]);
        dup2(mypipe[1][1], 1);
      }
    }

    if (command->next != NULL && command->next->in == TpipeErr)
    {

      dup2(1, 2);
      if (strcmp(get_flag(), "True") == 0)
      {
        close(mypipe[0][0]);
        dup2(mypipe[0][1], 1);
      }
      else
      {
        close(mypipe[1][0]);
        dup2(mypipe[1][1], 1);
      }
    }
  }

}

static void close_fd()
{
  close(fd);
  close(1);
  close(2);
}

static void run_command(Cmd command)
{
  int input_file = NO_FILE;
  fd = 0;
  char buffer[10];
  char *c_path = is_exists_cmd(command);
  pid_t p_id = fork();
  if (p_id == 0)
  {
    if (c_path == NULL)
    {
      if (command->out == ToutErr)
      {
        dup2(1, 2);
        fd = open(command->outfile, O_WRONLY | O_TRUNC | O_CREAT, 0660);
        dup2(fd, 1);
      }
      else if (command->out == TappErr) {
        dup2(1, 2);
        fd = open(command->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
        dup2(fd, 1);
      }
      printf("Command not found\n");
      fflush(stdout);
      if (fd > 0)
      {
        close_fd();
      }
      clearerr(stdout);
      exit(0);
    }
    else
    {
      setup(command, &input_file);
      //Command Execution
      int executed = execv(c_path, command->args);
      if (executed < 0)
      {
        perror("execv: ");
      }

      if (fd > 0)
      {
        close_fd();
      }

      if (input_file > 0)
      {
        close(input_file);
        close(0);
      }
      exit(0);
    }
  }
  //parent execution
  else
  {
    wait();
    main_handler(command);
  }
}


//_______________Parent Handler Routine_________________//
static void main_handler(Cmd command) {

  if (command->next != NULL && command->in == Tnil) {
    if (command->next->in == Tpipe || command->next->in == TpipeErr)
    {
      close(mypipe[1][1]);
      set_flag(1);
    }
  }
  else if (command->next != NULL && (command->in == Tpipe || command->in == TpipeErr))
  {
    if (strcmp(get_flag(), "True") == 0)
    {
      close(mypipe[0][1]);
      set_flag(0);
    }
    else
    {
      close(mypipe[1][1]);
      set_flag(1);
    }
  }

  if (command->next != NULL && (command->in == Tpipe || command->in == TpipeErr) && (command->next->in == Tpipe || command->next->in == TpipeErr))
  {
    if (strcmp(get_flag(), "True") == 0)
      open_pipe(mypipe[0]);
    else
      open_pipe(mypipe[1]);
  }
}

static void user_cd(Cmd command) {
  char directory[100] = "";
  char buff;
  fd;
  int input_file = NO_FILE;
  int i = 0;
  if (command->next == NULL)
  {
    if (command->in == Tin)
    {
      fd = open(command->infile, O_RDONLY, 0660);
      while (read(fd, &buff, 1) > 0 && buff != '\0' && buff != '\n')
      {
        directory[i] = buff;
        i++;
      }

      close(fd);

      if (chdir(directory) != 0)
        perror("-ush: cd: failed: ");
    }
    else if (command->args[1] != NULL)
    {
      if (chdir(command->args[1]) != 0)
        perror("-ush: cd: failed: ");
    }
    else
    {
      chdir(getenv("HOME"));
    }
  }
  else if (command->next != NULL)
  {
    pid_t pid = fork();
    if (pid == 0)
    {
      setup(command, &input_file);
      if (command->in == Tin)
      {
        fd = open(command->infile, O_RDONLY, 0660);
        while (read(fd, &buff, 1) > 0 && buff != '\n' && buff != '\0')
        {
          directory[i] = buff;
          i++;
        }

        close(fd);

        if (chdir(directory) != 0)
          perror("-ush: cd: failed: ");
      }
      else if (command->args[1] != NULL)
      {

        if (chdir(command->args[1]) != 0)
          perror("-ush: cd: failed: ");
      }
      else {
        chdir(getenv("HOME"));
      }
      exit(0);
    }
    else {
      wait();
      main_handler(command);
    }
  }
}



static void user_echo(Cmd command) {
  int fd_echo;
  char buff;
  int i = 0;
  int input_file = 0;
  pid_t pid;
  char *value;
  if (command->next != NULL) {
    if (pid = fork() == 0)
    {
      setup(command, &input_file);
      if (command->in == Tin)
      {
        printf("\n");
        fflush(stdout);
      }
      else if (command->in == Tnil)
      {
        for (i = 1; i < command->nargs; i++)
        {
          if ( strncmp(command->args[i], "$", 1) == 0 )
          {
            char *vrbl;
            vrbl = (char *)malloc(sizeof(strlen(command->args[i])));
            if (vrbl == NULL)
            {
              printf("Echo Command: Error allocating memory\n");
              exit(0);
            }
            memcpy( vrbl, (command->args[i]) + 1, ( strlen(command->args[i]) - 1 ) );
            vrbl[strlen(command->args[i])] = '\0';
            value = getenv(vrbl);
            if (value != NULL)
            {
              printf("%s ", value);
            }
            else
            {
              printf("%s: Undefined variable", command->args[i]);
            }
          }
          else
          {
            if ( i != (command->nargs - 1))
              printf("%s ", command->args[i]);
            else
              printf("%s", command->args[i]);

          }
        }
        printf("\n");
      }

      if (fd > 0)
      {
        close_fd();
      }

      if (input_file > 0)
      {
        close(input_file);
        close(0);
      }
      exit(0);
    }

    //parent execution
    else
    {
      wait();
      main_handler(command);
      return;
    }
  }
  else {

    fflush(stdout);
    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    if (command->out == ToutErr) {
      dup2(1, 2);
      fflush(stdout);
      fd = creat(command->outfile, 0660);
      dup2(fd, 1);
    }
    if (command->out == Tout) {
      fflush(stdout);
      fd = creat(command->outfile, 0660);
      dup2(fd, 1);
    }
    if (command->out == TappErr) {
      dup2(1, 2);
      fflush(stdout);
      fd = open(command->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
      dup2(fd, 1) == -1;
    }
    if (command->out == Tapp) {
      fflush(stdout);
      fd = open(command->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
      dup2(fd, 1) == -1;
    }


    if (command->in == Tin)
    {
      printf("\n");
      fflush(stdout);
    }
    else //if (command->in == Tnil)
    {
      for (i = 1; i < command->nargs; i++)
      {
        if ( strncmp(command->args[i], "$", 1) == 0 )
        {
          char *vrbl;
          vrbl = (char *)malloc(sizeof(strlen(command->args[i])));
          if (vrbl == NULL)
          {
            printf("Echo Command: Error allocating memory\n");
            exit(0);
          }
          memcpy( vrbl, (command->args[i]) + 1, ( strlen(command->args[i]) - 1 ) );
          vrbl[strlen(command->args[i])] = '\0';
          value = getenv(vrbl);
          if (value != NULL)
          {
            printf("%s ", value);
          }
          else
          {
            printf("%s: Undefined variable", command->args[i]);
          }
        }
        else
        {
          if ( i != (command->nargs - 1))
            printf("%s ", command->args[i]);
          else
            printf("%s", command->args[i]);

        }
      }
      printf("\n");
    }

    if (fd > 0)
    {
      close_fd();
    }

    if (input_file > 0)
    {
      close(input_file);
      close(0);
    }

    int dup_done = dup2(saved_in, STDIN_FILENO);
    if (dup_done < 0) {
      perror("input restore failed: ");
      exit(0);
    }
    int dup2_done = dup2(saved_out, STDOUT_FILENO);
    if (dup2_done < 0) {
      perror("input restore failed: ");
      exit(0);
    }
    close(saved_out);
    close(saved_in);
    fflush(stdout);
  }
}
static void user_setenv(Cmd command) {
  int input_file = 0;
  int fdset, i;
  char buf = 'a';
  char dir[100] = "";
  char *env, var[100], wor[100];


  if (command->next != NULL)
  {
    pid_t pid = fork();
    if (pid == 0)
    {
      setup(command, &input_file);
      if (command->in == Tpipe || command->in == TpipeErr)
      {
        if (flag == 0)
        {
          close(mypipe[0][1]);
          dup2(mypipe[0][0], 0);
        }
        else
        {
          close(mypipe[1][1]);
          dup2(mypipe[1][0], 0);

        }
      }

      if (command->next != NULL && (command->next->in == Tpipe || command->next->in == TpipeErr))
      {
        if (command->next->in == TpipeErr)
          dup2(1, 2);
        if (flag == 1)
        {

          close(mypipe[0][0]);

          dup2(mypipe[0][1], 1);
          //close(mypipe[0][1]);
        }
        else if (flag == 0)
        {
          close(mypipe[1][0]);
          dup2(mypipe[1][1], 1);
          //close(pipeB[1]);
        }
      }
      if (command->in == Tpipe || command->in == TpipeErr)
      {
        i = 0;
        while (read(0, &buf, 1) > 0 && buf != ' ')
        {
          dir[i] = buf;
          i++;
        }
        strcpy(var, dir);
        i = 0;
        while (read(0, &buf, 1) > 0 && buf != '\0' && buf != '\n')
        {
          dir[i] = buf;
          i++;
        }
        strcpy(wor, dir);

        if (setenv(var, wor, 1) != 0)
          perror("setenv error: ");
      }
      if (command->in == Tin)
      {
        fdset = open(command->infile, O_RDONLY, 0660);
        i = 0;
        while (read(fdset, &buf, 1) > 0 && buf != ' ')
        {
          dir[i] = buf;
          i++;
        }
        strcpy(var, dir);
        i = 0;
        while (read(fdset, &buf, 1) > 0 && buf != '\0' && buf != '\n')
        {
          dir[i] = buf;
          i++;
        }
        strcpy(wor, dir);
        if (setenv(var, wor, 1) != 0)
          perror("setenv error: ");
      }
      else
      {
        if (command->args[1] == NULL)
        {
          int i = 1;
          char *s = *environ;

          for (; s; i++) {
            printf("%s\n", s);
            s = *(environ + i);
          }
        }
        else
        {
          if (setenv(command->args[1], command->args[2], 1) != 0)
            perror("setenv error: ");
        }
      }
      exit(0);
    }
    else
    {
      wait();
      main_handler(command);
      return;
    }
  }
  else if (command->next == NULL) {

    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    if (command->out == ToutErr) {
      dup2(1, 2);
      fflush(stdout);
      fd = creat(command->outfile, 0660);
      dup2(fd, 1);
    }
    if (command->out == Tout) {
      fflush(stdout);
      fd = creat(command->outfile, 0660);
      dup2(fd, 1);
    }
    if (command->out == TappErr) {
      dup2(1, 2);
      fflush(stdout);
      fd = open(command->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
      dup2(fd, 1) == -1;
    }
    if (command->out == Tapp) {
      fflush(stdout);
      fd = open(command->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
      dup2(fd, 1) == -1;
    }


    if (command->in == Tin)
    {
      fdset = open(command->infile, O_RDONLY, 0660);
      i = 0;
      while (read(fdset, &buf, 1) > 0)
      {
        if (buf == ' ')
          break;
        dir[i] = buf;
        i++;
      }
      strcpy(var, dir);
      i = 0;
      while (read(fdset, &buf, 1) > 0 && buf != '\0' && buf != '\n')
      {
        dir[i] = buf;
        i++;
      }
      strcpy(wor, dir);
      if (setenv(var, wor, 1) != 0)
        perror("setenv error: ");
    }
    else
    {

      if (command->args[1] == NULL)
      {
        int i = 1;
        char *s = *environ;

        for (; s; i++) {
          printf("%s\n", s);
          s = *(environ + i);
        }
      }
      else
      {
        //printf("Main execute\n");
        if (setenv(command->args[1], command->args[2], 1) != 0)
          perror("setenv error: ");
      }
    }

    int dup_done = dup2(saved_in, STDIN_FILENO);
    if (dup_done < 0) {
      perror("input restore failed: ");
      exit(0);
    }
    int dup2_done = dup2(saved_out, STDOUT_FILENO);
    if (dup2_done < 0) {
      perror("input restore failed: ");
      exit(0);
    }
    close(saved_out);
    close(saved_in);
    fflush(stdout);

  }

}
static void user_unsetenv(Cmd command) {
  int fdset;
  int input_file = 0;
  char buf, dir[100] = "";
  int i = 0;
  pid_t pid = fork();
  if (command->next != NULL)
  {
    if (pid == 0)
    {
      setup(command, &input_file);
      if (command->in == Tin)
      {
        fdset = open(command->infile, O_RDONLY, 0660);
        while (read(fdset, &buf, 1) > 0 && buf != '\n' && buf != '\0')
        {
          dir[i] = buf;
          i++;
        }

        close(fdset);
        if (unsetenv(dir) != 0)
          perror("unset error: ");

      }
      else
      {

        if (command->args[1] != NULL)
        {
          if (unsetenv(command->args[1]) != 0)
            perror("unset error: ");
        }
        else
          printf("less no of arguments to unset: \n");
      }
      exit(0);
    }
    else {
      wait();
      main_handler(command);
    }
  }
  else {
    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
    setup(command, &input_file);
    if (command->in == Tin)
    {
      fdset = open(command->infile, O_RDONLY, 0660);
      while (read(fdset, &buf, 1) > 0 && buf != '\n' && buf != '\0')
      {
        dir[i] = buf;
        i++;
      }

      close(fdset);
      if (unsetenv(dir) != 0)
        perror("unset error: ");

    }
    else
    {

      if (command->args[1] != NULL)
      {
        if (unsetenv(command->args[1]) != 0)
          perror("unset error: ");
      }
      else
        printf("less no of arguments to unset: \n");

    }

    int dup_done = dup2(saved_in, STDIN_FILENO);
    if (dup_done < 0) {
      perror("input restore failed: ");
      exit(0);
    }
    int dup2_done = dup2(saved_out, STDOUT_FILENO);
    if (dup2_done < 0) {
      perror("input restore failed: ");
      exit(0);
    }
    close(saved_out);
    close(saved_in);
    fflush(stdout);
  }
}


static void user_pwd(Cmd command) {

  char *pw;
  long size;
  char *ptr;
  fd = 0;
  pid_t pid = fork();
  if (pid == 0)
  {
    if (command->next != NULL && (command->next->in == Tpipe || command->next->in == TpipeErr))
    {
      if (command->next->in == TpipeErr)
        dup2(1, 2);
      if (flag == 1)
      {
        close(mypipe[0][0]);
        dup2(mypipe[0][1], 1);
        close(mypipe[0][1]);
      }
      else if (flag == 0)
      {
        close(mypipe[1][0]);
        dup2(mypipe[1][1], 1);
        close(mypipe[1][1]);
      }
    }
    if (command->out == ToutErr || command->out == TappErr)
    {
      dup2(1, 2);
    }
    if (command->out == Tout || command->out == ToutErr)
    {
      fflush(stdout);
      fd = open(command->outfile, O_WRONLY | O_TRUNC | O_CREAT, 0660);
      if (dup2(fd, 1) == -1)
      {
        perror("DUP: ");
        exit(0);
      }
    }
    if (command->out == Tapp)
    {
      fd = open(command->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
      dup2(fd, 1);
    }
    //printf("HELLO");
    size = pathconf(".", _PC_PATH_MAX);
    if ((pw = (char *)malloc((size_t)size)) != NULL)
      ptr = getcwd(pw, (size_t)size);
    printf("%s\n", pw);

    if (fd > 0)
    {
      close(fd);
    }
    exit(0);
  }
  else
  {
    wait();
    main_handler(command);
  }

}

static void user_where(Cmd command) {
  char *c_path;
  char *comm;
  int i = 0, fdcd;
  char dir[100] = "", buff;
  pid_t pid = fork();
  if (pid == 0)
  {
    if (command->in == Tpipe || command->in == TpipeErr)
    {
      if (flag == 0)
      {
        close(mypipe[0][1]);
        dup2(mypipe[0][0], 0);
      }
      else if (flag == 1)
      {
        close(mypipe[1][1]);
        dup2(mypipe[1][0], 0);
      }
    }

    if (command->next != NULL && (command->next->in == Tpipe || command->next->in == TpipeErr))
    {
      if (command->next->in == TpipeErr)
        dup2(1, 2);

      if (flag == 1)
      {
        close(mypipe[0][0]);
        dup2(mypipe[0][1], 1);

      }
      else if (flag == 0)
      {
        close(mypipe[1][0]);
        dup2(mypipe[1][1], 1);

      }
    }
    if (command->out == ToutErr || command->out == TappErr)
    {
      dup2(1, 2);
    }
    if (command->out == Tout || command->out == ToutErr)
    {
      fflush(stdout);
      fd = creat(command->outfile, 0660);
      dup2(fd, 1);
    }
    if (command->out == Tapp || command->out == TappErr)
    {
      fflush(stdout);
      fd = open(command->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
      dup2(fd, 1);
    }

    if (command->in == Tpipe || command->in == TpipeErr)
    {
      i = 0;
      while (read(0, &buff, 1) > 0 && buff != '\n' && buff != '\0')
      {
        dir[i] = buff;
        i++;
      }
      comm = dir;
    }

    if (command->in == Tin)
    {
      fdcd = open(command->infile, O_RDONLY, 0660);
      i = 0;
      while (read(fdcd, &buff, 1) > 0 && buff != '\n' && buff != '\0')
      {
        dir[i] = buff;
        i++;
      }
      comm = dir;
      close(fdcd);
    }

    if (command->in == Tnil)
    {
      comm = command->args[1];
    }
    Cmd cmd = malloc(sizeof(cmd));
    cmd->args = malloc(sizeof(char *) * 10);
    cmd->args[0] = comm;
    c_path = is_exists_cmd(cmd);
    if (c_path != NULL)
      printf("%s\n", c_path);
    if (strcmp(comm, "cd") == 0)
      printf("%s: built-in command\n", comm);
    if (strcmp(comm, "echo") == 0)
      printf("%s: built-in command\n", comm);
    if (strcmp(comm, "setenv") == 0)
      printf("%s: built-in command\n", comm);
    if (strcmp(comm, "unsetenv") == 0)
      printf("%s: built-in command\n", comm);
    if (strcmp(comm, "pwd") == 0)
      printf("%s: built-in command\n", comm);
    if (strcmp(comm, "where") == 0)
      printf("%s: built-in command\n", comm);
    if (strcmp(comm, "logout") == 0)
      printf("%s: built-in command\n", comm);
    if (c_path == NULL)
      printf("%s: built-in command\n", comm);

    if (fd > 0)
    {
      close(fd);
    }

    exit(0);
  }


  else
  {
    wait();
    main_handler(command);
  }

}

static void user_nice(Cmd command) {

  char *cmd;
  int value;
  int input_file = 0;
  long int priority;
  setup(command, &input_file);
  pid_t parent = getpid();
  int child_pid = 0;
  int test;
  if (command->nargs == 1) {
    int success = setpriority(PRIO_PROCESS, parent, 4);
    if (success < 0) {
      perror("Unable to set priority: ");
    }
  }

  else if (command->nargs == 2)
  {
    priority = atoi(command->args[1]);
    if (priority > 19 || priority < -20)
    {
      printf("Out of range: ", command->args[1]);
      return;
    }
    else if (priority == 0)
    {
      if (command->args[1][0] == '+')
        setpriority(PRIO_PROCESS, parent, priority);
      else if (command->args[1][0] == '-')
        setpriority(PRIO_PROCESS, parent, priority);
      else
      {
        child_pid = fork();
        if (child_pid == 0)
        {
          if (setpriority(PRIO_PROCESS, getpid(), 4) < 0)
          {
            perror("Nice error: ");
            exit(0);
          }
          test = execvp(command->args[1], &command->args[1]);
          if (test != 0) {
            perror("Nice error: ");
            exit(0);
          }
          exit(0);
        }
        else
        {
          priority = 4;
          wait();
        }
      }
    }
    else
    {
      if (setpriority(PRIO_PROCESS, parent, priority) != 0)
      {
        perror("Nice error: ");
        exit(0);
      }
    }
  }
  else if (command->nargs == 3)
  {
    priority = atoi(command->args[1]);
    if (priority > 19 || priority < -20)
    {
      printf("Out of range: ", command->args[1]);
      return;
    }
    else if (priority == 0)
    {
      if (command->args[1][0] == '-')
      {
        child_pid = fork();
        if (child_pid == 0)
        {
          if (setpriority(PRIO_PROCESS, getpid(), 4) < 0)
          {
            perror("Nice error: ");
            exit(0);
          }
          execvp(command->args[1], &command->args[1]);
          exit(0);
        }
        else
        {
          priority = 4;
          wait();
        }
      }

      else if (command->args[1][0] == '+')
      {
        child_pid = fork();
        if (child_pid == 0)
        {
          if (setpriority(PRIO_PROCESS, getpid(), 4) < 0)
          {
            perror("Nice error: ");
            exit(0);
          }
          execvp(command->args[1], &command->args[1]);
          exit(0);
        }
        else
        {
          priority = 4;
          wait();
        }
      }
    }
    else {
      child_pid = fork();
      if (child_pid == 0) {
        if (setpriority(PRIO_PROCESS, getpid(), 4) < 0)
        {
          perror("Nice error: ");
          exit(0);
        }
        execvp(command->args[1], &command->args[1]);

      }
      else
      {
        priority = 4;
        wait();
      }
    }
  }
  fflush(stdout);
  main_handler(command);
  if (fd > 0)
  {
    close_fd();
  }

  if (input_file > 0)
  {
    close(input_file);
    close(0);
  }
}

static void do_presets(Cmd command) {
  char *option = command->args[0];
  if (strcmp(option, "logout") == 0)
    exit(0);
  else if (strcmp(option, "cd") == 0)
    user_cd(command);
  else if (strcmp(option, "echo") == 0)
    user_echo(command);
  else if (strcmp(option, "setenv") == 0)
    user_setenv(command);
  else if (strcmp(option, "unsetenv") == 0)
    user_unsetenv(command);
  else if (strcmp(option, "pwd") == 0)
    user_pwd(command);
  else if (strcmp(option, "nice") == 0)
    user_nice(command);
  else if (strcmp(option, "where") == 0)
    user_where(command);
}

static void exec_cmd(Cmd command) {

  if (strcmp(command->args[0], "cd") == 0 ||
      strcmp(command->args[0], "echo") == 0 ||
      strcmp(command->args[0], "setenv") == 0 ||
      strcmp(command->args[0], "unsetenv") == 0 ||
      strcmp(command->args[0], "pwd") == 0 ||
      strcmp(command->args[0], "nice") == 0 ||
      strcmp(command->args[0], "where") == 0 ||
      strcmp(command->args[0], "logout") == 0)
  {
    do_presets(command);
  }
  else
  {
    run_command(command);
  }
}


static char* is_exists_cmd(Cmd command) {
  char *str = malloc(100);
  struct stat st;
  char *result = malloc(100);
  char *cmd = malloc(100), *cm = malloc(100);
  char env[100];
  strcpy(cmd, "/");
  strcpy(env, getenv("PATH"));
  strcat(cmd, command->args[0]);
  result = strtok( env, ":");
  while ( result != NULL )
  {
    strcpy(str, result);
    strcat(str, cmd);
    if (stat(str, &st) == 0)
      return str;

    result = strtok( NULL, ":" );
  }
  return NULL;
}

static void prPipe(Pipe p)
{
  int i = 0;
  Cmd c;

  if ( p == NULL )
    return;
  for ( c = p->head; c != NULL; c = c->next ) {
    exec_cmd(c);
  }
  flag = 0;
  open_pipe(mypipe[0]);
  open_pipe(mypipe[1]);
  prPipe(p->next);
}

static void open_pipe(int p[]) {

  if (pipe(p) == -1)
    perror("pipe error: ");

}

int main(int argc, char *argv[])
{
  Pipe p;
  gethostname(host, 20);
  char *ush_path = malloc(sizeof(char) * 50);
  char *old_home;
  int fd;

  //Signal Handlers

  signal (SIGINT, int_handler);
  signal (SIGQUIT, quit_handler);
  signal (SIGTERM, term_handler);

  old_home = getenv("HOME");

  strcpy(ush_path, old_home);

  strcat(ush_path, "/.ushrc");
  if ((fd = open(ush_path, O_RDONLY)) == -1)
    perror("");

  else {

    int oldStdIn = dup(STDIN_FILENO);
    dup2(fd, STDIN_FILENO);
    close(fd);
    flag = 0;
    open_pipe(mypipe[0]);
    open_pipe(mypipe[1]);

    p = parse();
    prPipe(p);
    freePipe(p);
    dup2(oldStdIn, STDIN_FILENO);
    close(oldStdIn);
    //fflush(stdout);
  }

  while ( 1 ) {
    flag = 0;
    open_pipe(mypipe[0]);
    open_pipe(mypipe[1]);
    printf("%s%% ", host);
    fflush(stdout);
    p = parse();
    prPipe(p);
    freePipe(p);
  }
}

/*........................ end of main.c ....................................*/
