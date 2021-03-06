/* Copyright (c) 1985-2012, B-Core (UK) Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following
conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in
   the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT 
NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#include "String_TYPE.h"

#include "Bool_TYPE.h"

#include "BToolkitd_Server.h"

#define Get4byteNum \
    ( ( ( unsigned char ) _in_buf [ _in_buf_ptr     ] * 16777216 ) + \
      ( ( unsigned char ) _in_buf [ _in_buf_ptr + 1 ] *    65536 ) + \
      ( ( unsigned char ) _in_buf [ _in_buf_ptr + 2 ] *      256 ) + \
      ( ( unsigned char ) _in_buf [ _in_buf_ptr + 3 ] ) )

#define Get3byteNum \
    ( ( ( unsigned char ) _in_buf [ _in_buf_ptr    ] *    65536 ) + \
      ( ( unsigned char ) _in_buf [ _in_buf_ptr + 1 ] *      256 ) + \
      ( ( unsigned char ) _in_buf [ _in_buf_ptr + 2 ] ) )

#define Get2byteNum \
    ( ( ( unsigned char ) _in_buf [ _in_buf_ptr     ] * 256 ) + \
      ( ( unsigned char ) _in_buf [ _in_buf_ptr + 1 ] ) )

#define Get1byteNum \
    ( ( unsigned char ) _in_buf [ _in_buf_ptr ] )



#define Put4byteNum(n) \
    _out_buf [ _out_buf_ptr     ] = ( unsigned char ) ( (n) / 16777216 ); \
    _out_buf [ _out_buf_ptr + 1 ] = ( unsigned char ) ( ((n) % 16777216) / 65536 ); \
    _out_buf [ _out_buf_ptr + 2 ] = ( unsigned char ) ( ((n) % 65536)    / 256 ); \
    _out_buf [ _out_buf_ptr + 3 ] = ( unsigned char ) ( (n) % 256 )

#define Put3byteNum(n) \
    _out_buf [ _out_buf_ptr     ] = ( unsigned char ) ( ((n) % 16777216) / 65536 ); \
    _out_buf [ _out_buf_ptr + 1 ] = ( unsigned char ) ( ((n) % 65536)    / 256 ); \
    _out_buf [ _out_buf_ptr + 2 ] = ( unsigned char ) ( (n) % 256 )

#define Put2byteNum(n) \
    _out_buf [ _out_buf_ptr     ] = ( unsigned char ) ( ((n) % 65536)    / 256 ); \
    _out_buf [ _out_buf_ptr + 1 ] = ( unsigned char ) ( (n) % 256 )

#define Put1byteNum(n) \
    _out_buf [ _out_buf_ptr     ] = ( unsigned char ) ( (n) % 256 )

int master_sock;
int slave_sock;

void
BToolkitd_INIT ( int * _rep, void * _lockfile, int _port, void * _bufsavefile )
{
  int                 on = 1;
  struct sockaddr_in  s_server;
  int                 lf;              /* file descriptor for pid/lock file */
  char                pid_buf [ 10 ];  /* buffer for pid                    */
  pid_t               f;               /* for fork                          */

  /***
  create master_sock
  ***/
  if ( ( master_sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == -1 ) {
    printf ( "\n  BToolkitd_INIT: socket failed - errno %d\n", errno );
    * _rep = FALSE;
    return;
  }

  /***
  setsocketopt
  ***/
  if ( ( setsockopt ( master_sock, SOL_SOCKET, SO_REUSEADDR,
		      ( char * ) &on, sizeof ( on ) ) ) == -1 ) {
    printf ( "\n  BToolkitd_INIT: setsocketopt failed - errno %d\n", errno );
    * _rep = FALSE;
    return;
  }

  /***
  bind to port
  ***/
  memset ( ( char * ) &s_server, 0, sizeof ( s_server ) );
  s_server.sin_family = AF_INET;
  s_server.sin_addr.s_addr = htonl ( INADDR_ANY );
  s_server.sin_port = htons ( _port );
  if ( bind ( master_sock, ( struct sockaddr * ) &s_server,
                                           sizeof ( s_server ) ) == -1 ) {
    printf ( "\n  BToolkitd_INIT: can't bind to port %d - errno %d\n", _port, errno );  
    * _rep = FALSE;
    return;
  }

  /***
  listen, with maximum backlog
  ***/
  if ( listen ( master_sock, SOMAXCONN ) == -1 ) {
    printf ( "\n  BToolkitd_INIT: listen failed - errno %d\n", errno );
    * _rep = FALSE;
    return;
  }

  /***
  operate in background
  ***/

#ifndef TEST_FLAG

  f = fork ();
  if ( f < 0 ) {        /* error */
    printf ( "\n  BToolkitd_INIT: fork failed - errno %d\n", errno );
    * _rep = FALSE;
    return;
  }
  else if ( f ) {       /* parent */
    exit ( 0 );
  }
                        /* child continues */
  /***
  detach from controlling tty
  ***/
/*  TIOCNOTT not in the Solaris 7 operating environment
  fd = open ( "/dev/tty", O_RDWR );
  ioctl ( fd, TIOCNOTTY, 0 );
  close ( fd );
*/
  
#else

 printf ( "\n  BToolkitd_Server: TEST_FLAG set - fork/detach suppressed ...\n" );

#endif


  /***
  change directory to /
  ***/
  /*
  chdir ( "/" );
  */
  /*
  printf ( "BToolkitd_Server: chdir / suppressed ...\n" );
  */

  /***
  set umask
  ***/
  umask ( 027 );
  

  /***
  set parent process group
  ***/
  setpgid ( 0, getpid () );


  /***
  acquire exclusive lock
  ***/
  lf = open ( ( char * ) _lockfile, O_RDWR | O_CREAT, 0640 );
  if ( lf < 0 ) {                              /* error */
    printf ( "\n  BToolkitd_INIT: unable to open %s O_RDWR | O_CREAT - errno %d\n",
                                                  ( char * ) _lockfile, errno );
    * _rep = FALSE;
    return;
  }

/*
  if ( flock ( lf, LOCK_EX | LOCK_NB ) ) {     /? can't obtain exclusive lock ?/
    printf ( "\n  BToolkitd_INIT: unable to obtain lock on %s - errno %d\n",
                                                 ( char * ) _lockfile, errno  );
    * _rep = FALSE;
    return;
  }
*/

  /***
  set _sav_buf;
  zero _num_sav, reset malloc_done
  ***/
  if ( strlen ( ( char * ) _bufsavefile ) < 256 ) {
    sprintf ( _sav_buf, ( char * ) _bufsavefile );
  }
  else {
    printf ( "\n  BToolkitd_INIT: _bufsavefile too long (%d >= 256)\n",
                                             strlen ( ( char * ) _bufsavefile ) );
    * _rep = FALSE;
    return;    
  }
  _num_sav = 0;
  malloc_done = 0;

  /***
  write pid to file & close
  ***/
  sprintf ( pid_buf, "%d", getpid () );
  write ( lf, pid_buf, strlen ( pid_buf ) );
  close ( lf );

  /***
  report server started via syslog
  ***/
  openlog ( "BToolkitd_Server", LOG_PID, LOG_DAEMON );
  setlogmask ( LOG_UPTO (  LOG_WARNING ) );
  syslog ( LOG_ERR, "Listening at port %d\n", _port );
  closelog ();

  printf ( "\n  BToolkitd_Server: listening at port %d\n\n", _port );

  _out_buf_ptr = 2;
  _in_buf_ptr = 2;
  * _rep = TRUE;
}

void
BToolkitd_ACCEPT ( int * _rep )
{
  struct sockaddr  addr;      /* address for connecting enity       */
  socklen_t              addr_len;  /* amount of space pointed to by addr */

  addr_len = (socklen_t) sizeof ( addr );
  
  /*ENC: while loop instead of recursive call */
  while(1)
  {
  	slave_sock = accept ( master_sock, ( struct sockaddr * ) &addr, &addr_len );
  	if ( slave_sock < 0 )
	{
  	  	/* ENC: Linux accept() differs from other BSD socket implementations
     	  	passing already-pending network errors on the new socket as an error code.
      	 	For reliable operation the application should detect the network errors:
      	 	ENETDOWN, EPROTO, ENOPROTOOPT, EHOSTDOWN, ENONET, EHOSTUNREACH, EOPNOTSUPP, and ENETUNREACH.
      	 	*/
    		if (
         	errno == EINTR ||
	 	errno == ENETDOWN ||
	 	errno == EPROTO ||
	 	errno == ENOPROTOOPT ||
	 	errno == EHOSTDOWN ||
	 	errno == ENONET ||
	 	errno == EHOSTUNREACH ||
	 	errno == EOPNOTSUPP ||
	 	errno == ENETUNREACH
		)
    		{
    	  		/*ENC - should sleep here */
    	  		sleep(1);
      	  		continue;
    		}
    		printf ( "\n  BToolkitd_ACCEPT: accept failed - errno %d\n", errno );
    		* _rep = FALSE;
    		break;
  	}
	else
	{
    		* _rep = TRUE;
		break;
	}
  }
}

void
BToolkitd_READ ( int * _rep, int * _pp )
{
  int read_len;    /* no. bytes actually read */

  /* ENC: should this be using recv with the MSG_WAITALL flag set? */ 

  read_len = read ( slave_sock, _in_buf, 2 );
  if ( read_len != 2 ) {
    printf ( "\n  BToolkitd_READ: read error 1\n  `read' returned %d not 2\n", read_len);
    * _pp = 0;
    * _rep = FALSE;
    return;
  }
  _in_buf_ptr = 0;
  _in_buf_len = Get2byteNum;
  if ( _in_buf_len > BToolkitd_ServerP2 ) {
    printf ( "\n  BToolkitd_READ: read error 2\n  client requested %d read (>%d)\n",
                                                           read_len, BToolkitd_ServerP2 );
    * _pp = 0;
    * _rep = FALSE;
    return;
  }
  read_len =  read ( slave_sock, _in_buf, BToolkitd_ServerP2 );
  if ( read_len != _in_buf_len ) {
    printf ( "\n  BToolkitd_READ: read error 3\n  `read' returned %d not %d\n",
                                                               read_len, _in_buf_len );
    * _pp = 0;
    * _rep = FALSE;
    return;
  }
  _in_buf_ptr = 0;
  * _pp = _in_buf_len;
  * _rep = TRUE;

#ifdef TEST_FLAG
{
  int n; printf ( "BToolkitd_READ (%d):\n    `", read_len );
  for ( n = 0 ; n < read_len ; n++ ) { printf ( "%3d ", ( unsigned char ) _in_buf [ n ] ); } printf ( "'\n" );
}
#endif

}

void
BToolkitd_WRITE ( int * _rep )
{
  int write_len;
  int out_buf_ptr0 = _out_buf_ptr - 2;

  _out_buf_ptr = 0;
  Put2byteNum(out_buf_ptr0);
  write_len = write ( slave_sock, _out_buf, out_buf_ptr0 + 2 );
  if ( write_len != out_buf_ptr0 + 2 ) {
   printf ( "\n  BToolkitd_WRITE :write error\n  `write' returned %d not %d\n",
                                                                    write_len, out_buf_ptr0 );
   * _rep = FALSE;
  }
  else {
   * _rep = TRUE;
  }
  _out_buf_ptr = 2;

#ifdef TEST_FLAG
{
  int n; printf ( "BToolkitd_WRITE (%d including 1st two length bytes):\n    `", write_len );
  for ( n = 0 ; n < write_len ; n++ ) { printf ( "%3d ", ( unsigned char ) _out_buf [ n ] ); } printf ( "'\n" );
}
#endif

}

void
BToolkitd_CLOSE ( int * _rep )
{
  if ( close ( slave_sock ) == -1 ) {
    printf ( "\n  BToolkitd_CLOSE: close failed - errno %d\n", errno );
    * _rep = FALSE;
  }
  else {
    * _rep = TRUE;
  }
}

void
BToolkitd_GET_TOK ( int * _tok, int _toksize )
{
  if ( ( _in_buf_ptr + _toksize ) <=  BToolkitd_ServerP3 ) {
    switch ( _toksize ) {
    case 1:
      * _tok = Get1byteNum;
      break;
    case 2:
      * _tok = Get2byteNum;
      break;
    case 3:
      * _tok = Get3byteNum;
      break;
    case 4:
      * _tok = Get4byteNum;
      break;
    }
    _in_buf_ptr += _toksize;
  }
  else {
    * _tok = 1;
  }
}


void
BToolkitd_GET_STR ( int * _ss )
{
  int i = 0;
  char *s = ( char * ) _ss;
  int ss_len = Get2byteNum;

  if ( ( _in_buf_ptr + 3 ) <=  BToolkitd_ServerP3 ) {
    _in_buf_ptr += 2;
    while ( ss_len ) {
      if ( i < 1000 )  { s [ i++ ] = _in_buf [ _in_buf_ptr ]; }
      if ( i == 1000 ) { s [ i++ ] = '\0'; }
      ss_len--;
      _in_buf_ptr++;
    }
  }
  else {
    s [ 0 ] = '\0';
  }
}


/* could even be inlined  
void
BToolkitd_VAL_STR ( int * _ss )
{
  int ss_len = Get2byteNum;

  _in_buf_ptr += 2 ;
  _ss  = (int *) &(_in_buf [ _in_buf_ptr ]) ;
  _in_buf_ptr += ss_len;
}
*/

void
BToolkitd_PUT_TOK ( int _tok, int _toksize )
{
  if ( ( _out_buf_ptr + _toksize ) <=  BToolkitd_ServerP3 ) {
    switch ( _toksize ) {
    case 1:
      Put1byteNum ( _tok );
      break;
    case 2:
      Put2byteNum ( _tok );
      break;
   case 3:
      Put3byteNum ( _tok );
      break;
    case 4:
      Put4byteNum ( _tok );
      break;
    }
    _out_buf_ptr += _toksize;
  }
}

void
BToolkitd_PUT_STR ( int * _ss )
{
  char *s = (char *)_ss;
  int ss_len;
  int i = 0;
  ss_len = strlen ( s );

  if ( ( _out_buf_ptr + ss_len + 3 ) <  BToolkitd_ServerP3 ) {
    Put2byteNum ( ss_len + 1 );
    _out_buf_ptr += 2;
    while ( ss_len ) {
       _out_buf [ _out_buf_ptr++ ] = s [ i++ ];
       ss_len--;
     }
    _out_buf [ _out_buf_ptr++ ] = '\0';
  }
}

void
BToolkitd_SET_IN_PTR ( int _ptr )
{
  _in_buf_ptr = _ptr;
}

void
BToolkitd_GET_IN_PTR ( int * _ptr )
{
  * _ptr = _in_buf_ptr;
}

void
BToolkitd_SET_OUT_PTR ( int _ptr )
{
  _out_buf_ptr = _ptr + 2;
}

void
BToolkitd_GET_OUT_PTR ( int * _ptr )
{
  * _ptr = _out_buf_ptr - 2;
}

void
BToolkitd_PUT_FILE ( int * _rep, void * _filename )
{
  struct stat stat_buf;
  FILE * filename_ptr;
  int c;
  
  if ( stat ( ( char * ) _filename, & stat_buf ) != 0 ) {
    * _rep = FALSE;
    return;
  }
  if ( ( _out_buf_ptr + stat_buf.st_size + 3 ) >=  BToolkitd_ServerP3 ) {
    printf ( "\n  BToolkitd_PUT_FILE: file %s too long - errno %d\n",
                                               ( char * ) _filename, errno );
    * _rep = FALSE;
    return;
  }
  if ( ( filename_ptr = fopen ( ( char * ) _filename, "r" ) ) == NULL ) {
    printf ( "\n  BToolkitd_PUT_FILE: unable to fopen %s \"r\" - errno %d\n",
                                               ( char * ) _filename, errno );
    * _rep = FALSE;
    return;
  }
  Put2byteNum ( stat_buf.st_size + 1 );
  _out_buf_ptr += 2;
  c = getc ( filename_ptr );
  while ( c != EOF ) {
    _out_buf [ _out_buf_ptr++ ] = ( unsigned char ) c;
    c = getc ( filename_ptr );
  }
  if ( ( fclose ( filename_ptr ) ) ) {
    printf ( "\n  BToolkitd_PUT_FILE: fclose failed - errno %d\n", errno );
    * _rep = FALSE;
  }
  _out_buf [ _out_buf_ptr++ ] = '\0';
  * _rep = TRUE;  
}

void
BToolkitd_SAV_BUF ( int * _rep, int * _num_saves )
{
  int write_len;
  char len_buf [ 2 ];

  if ( ( _sav_buf_ptr = open ( _sav_buf, O_RDWR | O_APPEND | O_CREAT, 0640 ) ) == -1 ) {
    printf ( "\n  BToolkitd_SAV_BUF: unable to open %s O_RDWR | O_APPEND | O_CREAT - errno %d\n",
                                                   _sav_buf, errno );
    * _num_saves = 0;
    * _rep = FALSE;
    return;
  }
  len_buf [ 0 ] = ( unsigned char ) ( _in_buf_len  / 256 );
  len_buf [ 1 ] = ( unsigned char ) ( _in_buf_len  % 256 );
  write_len = write ( _sav_buf_ptr, len_buf, 2 );
  if ( write_len != 2 ) {
   printf ( "\n  BToolkitd_SAV_BUF: write error\n  `write' returned %d not %d\n",
                                                          write_len, 2 );
    * _num_saves = 0;
    * _rep = FALSE;
    return;
  }
  write_len = write ( _sav_buf_ptr, _in_buf, _in_buf_len );
  if ( write_len != _in_buf_len ) {
   printf ( "\n  BToolkitd_SAV_BUF: write error\n  `write' returned %d not %d\n",
                                                          write_len, _in_buf_len );
    * _num_saves = 0;
    * _rep = FALSE;
    return;
  }
  if ( close ( _sav_buf_ptr ) == -1 ) {
    printf ( "\n  BToolkitd_SAV_BUF: close failed - errno %d\n", errno );
    * _num_saves = 0;
    * _rep = FALSE;
    return;
  }
  _num_sav++;
  * _num_saves = _num_sav;
  * _rep = TRUE;
}

void
BToolkitd_SAV_RMV ()
{
  unlink ( _sav_buf );
  _num_sav = 0;
  if ( malloc_done ) {
    free ( malloc_buf );
    malloc_done = 0;
  }
}

void
BToolkitd_RST_BUF ( int * _rep )
{
  struct stat stat_buf;
  int read_len;
  
  malloc_buf_len = 0;
  if ( stat ( _sav_buf, & stat_buf ) != 0 ) {
    * _rep = FALSE;
    return;
  }
  malloc_buf_len = stat_buf.st_size;
  if ( ( _sav_buf_ptr = open ( _sav_buf, O_RDONLY ) ) == -1 ) {
    printf ( "\n  BToolkitd_RST_BUF: unable to open %s O_RDONLY - errno %d\n",
                                                   _sav_buf, errno );
    * _rep = FALSE;
    return;
  }
  if ( ! ( malloc_buf = malloc ( ( unsigned char ) stat_buf.st_size ) ) ) {
    printf ( "\n  BToolkitd_RST_BUF: unable to malloc - errno %d\n", errno );
    * _rep = FALSE;
    return;
  }
  read_len = read ( _sav_buf_ptr, malloc_buf, stat_buf.st_size );
  if ( read_len != stat_buf.st_size ) {
    printf ( "\n  BToolkitd_RST_BUF: read error 3\n  `read' returned %d not %d\n",
                                              read_len, ( int ) stat_buf.st_size );
    free ( malloc_buf );
    malloc_done = 0;
    * _rep = FALSE;
    return;
  }
  if ( close ( _sav_buf_ptr ) == -1 ) {
    printf ( "\n  BToolkitd_RST_BUF: close failed - errno %d\n", errno );
    free ( malloc_buf );
    malloc_done = 0;
    * _rep = FALSE;
    return;
  }
  malloc_buf_len = stat_buf.st_size;
  malloc_buf_ptr = 0;
  malloc_done = 1;
  * _rep = TRUE;

#ifdef TEST_FLAG
{ int n; printf ( "BToolkitd_RST_BUF (%d):\n    `", malloc_buf_len );
         for ( n = 0 ; n < malloc_buf_len ; n++ ) { printf ( "%3d ", ( unsigned char ) malloc_buf [ n ] ); } printf ( "'\n    `" );
         for ( n = 0 ; n < malloc_buf_len ; n++ ) { printf ( "%3c ", malloc_buf [ n ] ); } printf ( "'\n" ); }
#endif

}

void
BToolkitd_NXT_BUF ( int * _rep )
{
  int next_rst_len;
#ifdef TEST_FLAG
  int tot_len;
#endif

  if ( malloc_buf_ptr + 2 > malloc_buf_len ) {
    free ( malloc_buf );
    malloc_done = 0;
    malloc_buf_len = 0;
    * _rep = FALSE;
    return;
  }
  _in_buf [ 0 ] = ( unsigned char ) malloc_buf [ malloc_buf_ptr++ ];
  _in_buf [ 1 ] = ( unsigned char ) malloc_buf [ malloc_buf_ptr++ ];
  _in_buf_ptr = 0;
  next_rst_len = Get2byteNum;
  if ( malloc_buf_ptr + next_rst_len > malloc_buf_len ) {
    free ( malloc_buf );
    malloc_done = 0;
    malloc_buf_len = 0;
    * _rep = FALSE;
    return;
  }

#ifdef TEST_FLAG
  tot_len = next_rst_len ;
#endif

  _in_buf_ptr = 0;
  while ( next_rst_len ) {
    _in_buf [ _in_buf_ptr++ ] = ( unsigned char ) malloc_buf [ malloc_buf_ptr++ ];
    next_rst_len--;
  }

  _in_buf_ptr = 0;
  * _rep = TRUE;

#ifdef TEST_FLAG
{ int n; printf ( "BToolkitd_NXT_BUF (%d):\n    `", tot_len  );
         for ( n = 0 ; n < tot_len ; n++ ) { printf ( "%3d ", ( unsigned char ) _in_buf [ n ] ); } printf ( "'\n    `" );
         for ( n = 0 ; n < tot_len ; n++ ) { printf ( "%3c ", _in_buf [ n ] ); } printf ( "'\n" ); }
#endif
}
