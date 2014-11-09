/*
 * file:        homework.c
 * description: Skeleton for homework 1
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, Jan. 2012
 * $Id: homework.c 500 2012-01-15 16:15:23Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uprog.h"

// ten arguments at most - from Piazza
#define NUM_ARGS 10
// 128 characters in the line, from Piazza
#define LINE_BUFFER_SIZE 128
// 20 characters in the argument, from Piazza
#define ARG_BUFFER_SIZE 20

static char argv[NUM_ARGS][ARG_BUFFER_SIZE];
static char line[LINE_BUFFER_SIZE];
static char command[ARG_BUFFER_SIZE];
/***********************************/
/* Declarations for code in misc.c */
/***********************************/

typedef int *stack_ptr_t;
extern void init_memory(void);
extern void do_switch(stack_ptr_t *location_for_old_sp, stack_ptr_t new_value);
extern stack_ptr_t setup_stack(int *stack, void *func);
extern int get_term_input(char *buf, size_t len);
extern void init_terms(void);

extern void  *proc1;
extern void  *proc1_stack;
extern void  *proc2;
extern void  *proc2_stack;
extern void **vector;



/***********************************************/
/********* Your code starts here ***************/
/***********************************************/

/*
 * Question 1.
 *
 * The micro-program q1prog.c has already been written, and uses the
 * 'print' micro-system-call (index 0 in the vector table) to print
 * out "Hello world".
 *
 * You'll need to write the (very simple) print() function below, and
 * then put a pointer to it in vector[0].
 *
 * Then you read the micro-program 'q1prog' into memory starting at
 * address 'proc1', and execute it, printing "Hello world".
 *
 */
void print(char *line) {
  printf("%s",line);
}

int readfile(char *file, void *buf) {
  FILE *fp = NULL;
  int lSize;

  fp = fopen ( file , "rb" );
  if( !fp ) {
    perror("can't open file");
    return 0;
  }

  fseek( fp , 0L , SEEK_END);
  lSize = ftell( fp );
  rewind( fp );

  if( !buf ) {
    fclose(fp);
    perror("memory alloc fails");
    return 0;
  }

  /* copy the file into the buffer */
  if( 1!=fread( buf , lSize, 1 , fp) ) {
    fclose(fp);
    free(buf);
    perror("entire read fails");
    return 0;
  }

  fclose(fp);
  return 1;

}

void q1(void) {
  vector[0] = &print;

  if (readfile("q1prog",proc1)) {
    void (*f)(void) = proc1;
    f();
  }
  else 
    printf("bad command: %s\n", command);
}


/*
 * Question 2.
 *
 * Add two more functions to the vector table:
 *   void readline(char *buf, int len) - read a line of input into 'buf'
 *   char *getarg(int i) - gets the i'th argument (see below)

 * Write a simple command line which prints a prompt and reads command
 * lines of the form 'cmd arg1 arg2 ...'. For each command line:
 *   - save arg1, arg2, ... in a location where they can be retrieved
 *     by 'getarg'
 *   - load and run the micro-program named by 'cmd'
 *   - if the command is "quit", then exit rather than running anything
 *
 * Note that this should be a general command line, allowing you to
 * execute arbitrary commands that you may not have written yet. You
 * are provided with a command that will work with this - 'q2prog',
 * which is a simple version of the 'grep' command.
 *
 * NOTE - your vector assignments have to mirror the ones in vector.s:
 *   0 = print
 *   1 = readline
 *   2 = getarg
 */
void readline(char *buf, int len) /* vector index = 1 */
{
  fgets(buf, len, stdin);
}

char *getarg(int i)		/* vector index = 2 */
{
  if (argv[i][0]) {
    return argv[i];
  } else
    return NULL;
}

/* find the next word starting at 's', delimited by characters
 * in the string 'delim', and store up to 'len' bytes into *buf
 * returns pointer to immediately after the word, or NULL if done.
 */
char *strwrd(char *s, char *buf, size_t len, char *delim) {
  s += strspn(s, delim);
  int n = strcspn(s, delim);  /* count the span (spn) of bytes in */
  if (len-1 < n)              /* the complement (c) of *delim */
    n = len-1;
  memcpy(buf, s, n);
  buf[n] = 0;
  s += n;
  return (*s == 0) ? NULL : s;
}

/*
 * Note - see c-programming.pdf for sample code to split a line into
 * separate tokens. 
 */
void q2(void) {
  vector[0] = &print;
  vector[1] = &readline;
  vector[2] = &getarg;

  char* s = NULL;
  while (1) {
    printf("> ");
    /* get a line */
    readline(line, sizeof(line));

    /* split it into words */
    //get command
    s = strwrd(line, command, sizeof(argv[0]), " \t\n");
    /* if zero words, continue */
    if (s == NULL) {
      continue;
    }
    /* if first word is "quit", break */
    if (!strcmp(command, "quit")) {
      break;
    }
    //get arguments
    int argc;

    for (argc = 0; argc < NUM_ARGS; argc++) {
      s = strwrd(s, argv[argc], sizeof(argv[argc]), " \t\n");
      /* make sure 'getarg' can find the remaining words */
      if (s == NULL)
        break; 
    }
    /* load and run the command */
    if(readfile(command,proc1)) {
      void (*f)(void) = proc1;
      f();
    } else 
      printf("bad command: %s\n", command);
  }
  /*
   * Note that you should allow the user to load an arbitrary command,
   * and print an error if you can't find and load the command binary.
   */
}

/*
 * Question 3.
 *
 * Create two processes which switch back and forth.
 *
 * You will need to add another 3 functions to the table:
 *   void yield12(void) - save process 1, switch to process 2
 *   void yield21(void) - save process 2, switch to process 1
 *   void uexit(void) - return to original homework.c stack
 *
 * The code for this question will load 2 micro-programs, q3prog1 and
 * q3prog2, which are provided and merely consists of interleaved
 * calls to yield12() or yield21() and print(), finishing with uexit().
 *
 * Hints:
 * - Use setup_stack() to set up the stack for each process. It returns
 *   a stack pointer value which you can switch to.
 * - you need a global variable for each process to store its context
 *   (i.e. stack pointer)
 * - To start you use do_switch() to switch to the stack pointer for 
 *   process 1
 */

static stack_ptr_t process1 = NULL, process2 = NULL, homework_stack = NULL;

void yield12(void)		/* vector index = 3 */
{
  do_switch(&process1, process2);
}

void yield21(void)		/* vector index = 4 */
{
  do_switch(&process2, process1);
}

void uexit(void)		/* vector index = 5 */
{
  do_switch(NULL, homework_stack);
}

void q3(void)
{
  vector[0] = &print;
  vector[1] = &readline;
  vector[2] = &getarg;
  vector[3] = &yield12;
  vector[4] = &yield21;
  vector[5] = &uexit;
  /* load q3prog1 into process 1 and q3prog2 into process 2 */

  if (!readfile("q3prog1", proc1)) {
    return;
  }

  if (!readfile("q3prog2", proc2)) {
    return;
  }

  process1 = setup_stack(proc1_stack, proc1);
  process2 = setup_stack(proc2_stack, proc2);

  do_switch(&homework_stack, process1);
}


/***********************************************/
/*********** Your code ends here ***************/
/***********************************************/
