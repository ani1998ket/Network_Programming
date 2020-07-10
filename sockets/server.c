#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


#define PORT "3490"         // Binding Port
#define BACKLOG 10          // Queue Count


const struct {
    char* s_socket;
    char* s_bind;
    char* s_failbind;

    char* setsockopt;
} EMSG = {  "server: socket",
            "server: bind",
            "server: failed to bind\n",
            "setsockopt",
};

void init_hints( struct addrinfo *hints );
void sigchld_handler( int s );
void* get_in_addr( struct sockaddr* sa );

int main( void ){

    int sockfd; 
    int connfd;
    
    struct addrinfo hints;
    struct addrinfo *servinfo, *temp;
    struct sockaddr_storage conn_addr;
    socklen_t sin_size;

    struct sigaction sa;

    int yes = 1;
    char s[ INET6_ADDRSTRLEN ];
    int status;

    init_hints( &hints );
    
    if( status = getaddrinfo( NULL, PORT, &hints, &servinfo ) != 0 ){
        fprintf( stderr, "getaddrinfo: %s\n", gai_strerror( status ) );
        return 1;
    }

    for( temp = servinfo; temp != NULL; temp = temp -> ai_next ){
        
        sockfd = socket( temp -> ai_family, temp -> ai_socktype, temp -> ai_protocol );
        if( sockfd == -1 ){
            perror( EMSG.s_socket );
            continue;
        }
        
        // Set Socket options at Socket Level
        if( setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1 ){        
            perror( EMSG.setsockopt );
            exit(1);
        }

        if( bind( sockfd, temp -> ai_addr, temp -> ai_addrlen ) == -1 ){
            close( sockfd );
            perror( EMSG.s_bind );
            continue;
        }

        break;
    }

    freeaddrinfo( servinfo );

    if( temp == NULL ){
        fprintf( stderr, EMSG.s_failbind );
    }

    if( listen( sockfd, BACKLOG ) == -1 ){
        perror( "listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; //Reap all dead processes
    sigemptyset( &sa.sa_mask );
    sa.sa_flags = SA_RESTART;
    if( sigaction( SIGCHLD, &sa, NULL ) == -1 ){
        perror( "sigaction" );
        exit(1);
    }

    printf( "server: waiting for connections...\n" );
    
    while(1){
       sin_size = sizeof conn_addr;
       connfd = accept( sockfd, (struct sockaddr*)&conn_addr, &sin_size );
       if( connfd == -1 ){
           perror( "accept");
           continue;
       }
       
       // Network( Binary ) to Printable( Text )
       inet_ntop( conn_addr.ss_family,
               get_in_addr(( struct sockaddr*) &conn_addr ),
               s, sizeof s);

       printf( "server: got connection from %s\n", s );

       if( !fork() ){
            // Child process
            close( sockfd ); // Child don't need the listener
            if( send( connfd, "Hello, World!\n", 14, 0) == -1 )
               perror( "send" );
            close( connfd );
            exit(0);
       }
       close( connfd );

    }
    return 0;
}

void init_hints( struct addrinfo *hint_ptr ){

    memset( hint_ptr , 0, sizeof *hint_ptr );

    hint_ptr -> ai_family   = AF_UNSPEC;
    hint_ptr -> ai_socktype = SOCK_STREAM;
    hint_ptr -> ai_flags    = AI_PASSIVE;

}

void sigchld_handler( int s ){

    int saved_errno = errno; 
    // return immediately if no child has exited( WITH NO HANG )
    while( waitpid( -1, NULL, WNOHANG ) > 0 ){;}
    errno = saved_errno;

}

void* get_in_addr( struct sockaddr* sa ){
    if( sa->sa_family == AF_INET ){
        return &( ((struct sockaddr_in*)sa) -> sin_addr );
    }

    return &( ((struct sockaddr_in6*)sa) -> sin6_addr );
}
