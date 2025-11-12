#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>

extern long next_thread_number;
extern int max_squares;
extern int total_open_tours;
extern int total_closed_tours;

pthread_mutex_t mutex_on_next_thread_number = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_on_max_squares = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_on_total_open_tours = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_on_total_closed_tours = PTHREAD_MUTEX_INITIALIZER;

// Struct to make it easier to pass location pairs
typedef struct
{
    int r;
    int c;
}location;

typedef struct
{
    location * moves;
    int ** board;
    location start;
    int count;
    int m;
    int n;
    long thread_num;
    char caller;
}info;

void free_board(int ** board, int m);
int length(location * moves);
int within_board(int m, int n, location loc);
int **initialize_board(int m, int n, int r, int c);
int ** copy_board( int ** board, int m, int n );
void print_board(int ** board, int m, int n);
int unvisited_count(int ** board, int m, int n);
location *check_valid_moves(int ** board, int m, int n, location pos, location start);
int contains(location loc, location *moves);
int dead_end_check(int ** board, int m, int n, location *moves);
int closed_tour_check(int ** board, int m, int n, location start, location *moves);
int open_tour_check(int ** board, int m, int n, location start, location *moves);
int process_moves( info data );
int solve( int argc, char ** argv );
void *thread_wrapper(void *arg);

void *thread_wrapper(void *arg) {
    info *data = (info *)arg;
    int result = process_moves(*data);  // Pass the data by value to process_moves
    return (void *)(intptr_t)result;  // Return result as void pointer
}

int length( location* moves )
{
    int count = 0;
    for( location* ptr = moves; (*ptr).r!= -100; ptr++ )
    {
        count++;
    }

    return count;
}

// Check if a location is within the board
int within_board( int m, int n, location loc )
{
    if ( ( loc.r >= 0 && loc.r < m) && ( loc.c >= 0 && loc.c < n) )
    {
        return 1;
    }

    return 0;
}

// Free mem allocated to board
void free_board( int ** board, int m)
{
    for( int i = 0; i < m; i++)
    {
        free( *(board + i) );
    }

    free( board );
}
// Board creation
int ** initialize_board( int m, int n, int r, int c)
{
    int ** board = calloc( m, sizeof( int *) );

    for( int i = 0; i < m; i++)
    {
        *(board + i) = calloc( n, sizeof( int ) );
    }
    
    // Marking off the start location as visited
    * ( *(board + r ) + c ) = 1; 

    return board;
}

void print_board(int ** board, int m, int n) {
    // Print the top border
    printf("+");
    for (int j = 0; j < n; j++) {
        printf("---+");
    }
    printf("\n");

    // Print the board content with grid lines
    for (int i = 0; i < m; i++) {
        printf("|");
        for (int j = 0; j < n; j++) {
            printf(" %d |", *( *(board + i) + j ) );
        }
        printf("\n");

        // Print the row separator
        printf("+");
        for (int j = 0; j < n; j++) {
            printf("---+");
        }
        printf("\n");
    }
}

// Find the number of unvisited squares in the board
int unvisited_count( int ** board, int m, int n )
{
    int count = 0;
    for( int i = 0; i < m; i++)
    {
        for( int j = 0; j < n; j++ )
        {
            if( *( *( board + i) + j ) == 0 )
            {
                count++;
            }
        }

    }

    return count;
}

// There are 8 possible moves from every location. Check which of them are valid and return
// an array with all valid moves.
location * check_valid_moves( int ** board, int m, int n, location pos, location start ) {
    location * offsets = calloc(8, sizeof(location));
    int valid_move_count = 0;
    *(offsets + 0) = (location){-2, 1};
    *(offsets + 1) = (location){-1, 2};
    *(offsets + 2) = (location){1, 2};
    *(offsets + 3) = (location){2, 1};
    *(offsets + 4) = (location){2, -1};
    *(offsets + 5) = (location){1, -2};
    *(offsets + 6) = (location){-1, -2};
    *(offsets + 7) = (location){-2, -1};

    location * offsetPtr = offsets;

    for (int i = 0; i < 8; i++, offsetPtr++) {
        location move = {pos.r + (*offsetPtr).r, pos.c + (*offsetPtr).c};

        // If the move is within the board and the square is unvisited update the valid move count
        if (within_board(m, n, move) && *(*(board + move.r) + move.c) != 1) 
        {
            valid_move_count++;
        }

        // Check for tours
        // If the move is within the board and all squares have been visited, then we have either an open tour or a closed tour
        // Thus increment the count

        if( within_board( m, n, move) && unvisited_count( board, m, n) == 0 && move.r == start.r && move.c == start.c)
        {
            valid_move_count++;
        }
    }

    location *moves = calloc( valid_move_count+1, sizeof(location) );
    location *movePtr = moves;

    offsetPtr = offsets;

    int counter = 0;

    for (int i = 0; i < 8; i++) {
        location move = {pos.r + (*(offsetPtr+i)).r, pos.c + (*(offsetPtr+i)).c};

        if( within_board( m, n, move) && unvisited_count( board, m, n) == 0 && move.r == start.r && move.c == start.c)
        {
            *( movePtr + counter ) = move;
            counter++;
        }

        else if (within_board(m, n, move) && *(*(board + move.r) + move.c) != 1) {
            *( movePtr + counter ) = move;
            counter++;
        }
        // Check for our tours
        // Since we found that we had a tour earlier, we now handle adding the move to the array
        // If the move we are considering is the same location as start, then we will add it to the array
        // The closed tour check will look for this. Despite the square being '1' (visited) 
        // For open tours, we will be checking for the lack of the start state in the array.
        // The start state will not be added to the array in the case of an open tour becuase the 
        // start state will not fall within the 8 possible moves.
    }
    free( offsets );
    *( moves + valid_move_count ) = (location) {-100, 0};
    return moves;
}

void print_moves(location * moves)
{
    for(location *ptr = moves; (*ptr).r != -100; ptr++)
    {
        printf("(%d,%d),",(*ptr).r,(*ptr).c);
    }
    printf("\n");
    return;
}

int contains( location loc, location * moves )
{
    int l = length( moves );
    for( int i = 0; i < l; i++ )
    {
        if( loc.r == (*(moves + i)).r && loc.c == (*(moves + i)).c)
        {
            return 1;
        }
    }

    return 0;
}

int dead_end_check( int ** board, int m, int n, location * moves )
{
    if( length( moves ) == 0 && unvisited_count( board, m, n ) > 0)
    {
        return 1;
    }

    return 0;
}

int closed_tour_check( int ** board, int m, int n, location start, location * moves )
{
    if( unvisited_count( board, m, n ) == 0 && contains( start, moves ) )
    {
        return 1;
    }

    return 0;
}

int open_tour_check( int ** board, int m, int n, location start, location * moves )
{
    if( unvisited_count( board, m, n ) == 0 && !contains( start, moves ) )
    {
        return 1;
    }

    return 0;
}

int ** copy_board( int ** board, int m, int n )
{
    int ** copy = calloc( m, sizeof( int *) );

    for( int i = 0; i < m; i++)
    {
        *(copy + i) = calloc( n, sizeof( int ) );
    }
    
    for( int i = 0; i < m; i++ )
    {
        for ( int j = 0; j < n; j++ )
        {
            *( *( copy + i ) + j )= *( *(board + i) + j );
        }
    }

    return copy;
}

int process_moves(info data) {

    location * moves = data.moves;
    int ** board = data.board;
    location start = data.start;
    int count = data.count;
    int m = data.m;
    int n = data.n;
    int move_size = length(moves);
    
    // Dead-end check
    if (dead_end_check(board, m, n, moves)) {
        free(moves);
        free_board(board, m);
        #ifndef QUIET
        printf( "T%ld: Dead end at move #%d\n", data.thread_num, count );
        #endif

        return count;
    }

    // Open tour check
    if (open_tour_check(board, m, n, start, moves)) {
        free(moves);
        free_board(board, m);
        pthread_mutex_lock( &mutex_on_total_open_tours );
        total_open_tours++;
        pthread_mutex_unlock( &mutex_on_total_open_tours );

        #ifndef QUIET
        printf("T%ld: Found an open knight's tour; incremented total_open_tours\n", data.thread_num);
        #endif

        return count;
    }

    // Closed tour check
    if (closed_tour_check(board, m, n, start, moves)) {
        free(moves);
        free_board(board, m);
        pthread_mutex_lock( &mutex_on_total_closed_tours );
        total_closed_tours++;
        pthread_mutex_unlock( &mutex_on_total_closed_tours );
    
        #ifndef QUIET
        printf("T%ld: Found a closed knight's tour; notifying top-level parent\n", data.thread_num);
        #endif

        return count;
    }

    // If only one move is possible, process it recursively
    if (move_size == 1) {
        location move = *(moves);
        *( *(board + move.r) + move.c ) = 1;

        location * new_moves = check_valid_moves(board, m, n, move, start);
        free(moves);
        info d = {new_moves, copy_board( board, m, n ), start, count+1, m, n, data.thread_num, data.caller};
        int result = process_moves( d );
        free_board(board, m);

        return result;
    }

    pthread_t * child_threads = calloc(move_size, sizeof(pthread_t));  // Track multiple threads
    info * new_data = calloc(move_size, sizeof(info));

    #ifndef QUIET
    if (data.caller == 'M') {
        if( move_size == 1)
        {
            printf("MAIN: %d possible moves after move #%d; creating %d child thread...\n", move_size, count, move_size);
        }
        else
        {
            printf("MAIN: %d possible moves after move #%d; creating %d child threads...\n", move_size, count, move_size);
        }

    } 
    
    else {
        if( move_size == 1)
        {
            printf("T%ld: %d possible move after move #%d; creating %d child thread...\n", data.thread_num, move_size, count, move_size);
        }
        else
        {
            printf("T%ld: %d possible moves after move #%d; creating %d child threads...\n", data.thread_num, move_size, count, move_size);
        }
    }

    #endif

    int max_count = count;  // Track the highest count found

    for (int i = 0; i < move_size; i++) {
        location move = *(moves + i);
        *(new_data + i) = data;
        (new_data + i )->board = copy_board( board, m, n );
        int ** new_board = (new_data+i)->board;
        *( *( new_board + move.r ) + move.c ) = 1;
        location * new_moves = check_valid_moves((new_data+i)->board, m, n, move, start);
        (new_data + i)->moves = new_moves;
        (new_data + i)->count = count+1;

        pthread_mutex_lock( &mutex_on_next_thread_number);
        (new_data + i)->thread_num = next_thread_number;
        next_thread_number++;
        pthread_mutex_unlock( &mutex_on_next_thread_number);

        if( data.caller == 'M')
        {    
            (new_data + i)->caller = 'm';
        }
        else if( data.caller == 'm')
        {
            (new_data + i)->caller = 't';
        }
        int rc = pthread_create(( child_threads + i ), NULL, thread_wrapper, (void *)(new_data + i));

        if (rc != 0) {
            fprintf(stderr, "pthread_create() failed (%d)\n", rc);
        }

        // Wait for the thread to finish before moving to the next
        #ifdef NO_PARALLEL
        void *retval;
        pthread_join(*(child_threads+i), &retval);
        #ifndef QUIET
        if ((new_data+i)->caller == 'M' || (new_data+i)->caller == 'm')
        {
            printf( "MAIN: T%ld joined\n", (new_data+i)->thread_num );
        }
        else
        {
            printf( "T%ld: T%ld joined\n",data.thread_num, (new_data+i)->thread_num );
        }
        #endif
        // pthread_mutex_lock( &mutex_on_next_thread_number);
        // next_thread_number++;
        // pthread_mutex_unlock( &mutex_on_next_thread_number);

        int thread_count = (int)(intptr_t)retval;  // Convert return value
        if (thread_count > max_count) {
            max_count = thread_count;
        }
        #endif
    }

    #ifndef NO_PARALLEL
    // Wait for all threads to complete in parallel mode
    for (int i = 0; i < move_size; i++) {
        void *retval;
        pthread_join(*(child_threads + i), &retval);

        #ifndef QUIET
        if ((new_data + i)->caller == 'M' || (new_data + i)->caller == 'm') {
            printf("MAIN: T%ld joined\n", (new_data+i)->thread_num);
        } else {
            printf("T%ld: T%ld joined\n", data.thread_num, (new_data+i)->thread_num);
        }
        #endif

        int thread_count = (int)(intptr_t)retval;
        if (thread_count > max_count) {
            max_count = thread_count;
        }
    }

    #endif
    count = max_count;  // Use the highest count from any child thread

    pthread_mutex_lock( &mutex_on_max_squares );
    if( count > max_squares ) {
        max_squares = count;
    }
    pthread_mutex_unlock( &mutex_on_max_squares );


    free(moves);
    free_board(board, m);
    free( child_threads );
    free( new_data );

    return count;
}



int solve( int argc, char ** argv )
{
    setvbuf( stdout, NULL, _IONBF, 0 );
    

    if( argc < 5 )
    {
        fprintf( stderr, "Invalid number of arguments.\n" );
        return EXIT_FAILURE;
    }

    int m = atoi( *(argv + 1) );
    int n = atoi( *(argv + 2) );
    int r = atoi( *(argv + 3) );
    int c = atoi( *(argv + 4) );

    if( m < 2 || n < 2 || r < 0 || r >= m || c < 0 || c >= n )
    {
        fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: ./hw3.out <m> <n> <r> <c>\n" );
        return EXIT_FAILURE;
    }

    int ** board = initialize_board(m, n, r, c);

    printf( "MAIN: Solving the knight's tour problem for a %dx%d board\n", m, n );
    printf( "MAIN: Starting at row %d and column %d (move #1)\n", r, c );

    location start = (location){r, c};
    
    // Initial list of valid moves  
    location * moves = check_valid_moves( board, m, n, start, start );

    info data = { moves, board, start, 1, m, n, next_thread_number, 'M' };

    process_moves( data );

    pthread_mutex_lock( &mutex_on_total_open_tours );
    pthread_mutex_lock( &mutex_on_total_closed_tours );

    if( total_open_tours > 0 || total_closed_tours > 0)
    {
        printf( "MAIN: Search complete; found %d open tours and %d closed tours\n", total_open_tours, total_closed_tours );
    }

    else
    {
        pthread_mutex_lock( &mutex_on_max_squares );
        printf( "MAIN: Search complete; best solution(s) visited %d squares out of %d\n", max_squares, m*n );
        pthread_mutex_unlock( &mutex_on_max_squares );
    }

    // pthread_mutex_lock( &mutex_on_next_thread_number );
    // printf( "MAIN: thread num is %ld\n", next_thread_number);
    // pthread_mutex_unlock( &mutex_on_next_thread_number );

    pthread_mutex_unlock( &mutex_on_total_open_tours );
    pthread_mutex_unlock( &mutex_on_total_closed_tours );

    return EXIT_SUCCESS;
}


