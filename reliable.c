/*
    reliable.io reference implementation

    Copyright © 2017, The Network Protocol Company, Inc.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

        2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
           in the documentation and/or other materials provided with the distribution.

        3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived 
           from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <reliable.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <math.h>

#define RELIABLE_ENABLE_LOGGING 1

#ifndef RELIABLE_ENABLE_TESTS
#define RELIABLE_ENABLE_TESTS 1
#endif // #ifndef RELIABLE_ENABLE_TESTS

// ------------------------------------------------------------------

static int log_level = 0;

void reliable_log_level( int level )
{
    log_level = level;
}

#if RELIABLE_ENABLE_LOGGING

void reliable_printf( int level, const char * format, ... ) 
{
    if ( level > log_level )
        return;
    va_list args;
    va_start( args, format );
    vprintf( format, args );
    va_end( args );
}

#else // #if RELIABLE_ENABLE_LOGGING

void reliable_printf( int level, const char * format, ... ) 
{
    (void) level;
    (void) format;
}

#endif // #if RELIABLE_ENABLE_LOGGING

// ---------------------------------------------------------------

int reliable_init()
{
    // ...

    return 1;
}

void reliable_term()
{
    // ...
}

// ---------------------------------------------------------------

int reliable_sequence_greater_than( uint16_t s1, uint16_t s2 )
{
    return ( ( s1 > s2 ) && ( s1 - s2 <= 32768 ) ) || 
           ( ( s1 < s2 ) && ( s2 - s1  > 32768 ) );
}

int reliable_sequence_less_than( uint16_t s1, uint16_t s2 )
{
    return reliable_sequence_greater_than( s2, s1 );
}

// ---------------------------------------------------------------

struct reliable_sequence_buffer_t
{
    uint16_t sequence;
    int num_entries;
    int entry_stride;
    uint32_t * entry_sequence;
    uint8_t * entry_data;
};

struct reliable_sequence_buffer_t * reliable_sequence_buffer_create( int num_entries, int entry_stride )
{
    assert( num_entries > 0 );
    assert( entry_stride > 0 );

    struct reliable_sequence_buffer_t * sequence_buffer = (struct reliable_sequence_buffer_t*) malloc( sizeof( struct reliable_sequence_buffer_t ) );

    sequence_buffer->sequence = 0;
    sequence_buffer->num_entries = num_entries;
    sequence_buffer->entry_stride = entry_stride;
    sequence_buffer->entry_sequence = (uint32_t*) malloc( num_entries * sizeof( uint32_t ) );
    sequence_buffer->entry_data = (uint8_t*) malloc( num_entries * entry_stride );

    return sequence_buffer;
}

void reliable_sequence_buffer_destroy( struct reliable_sequence_buffer_t * sequence_buffer )
{
    assert( sequence_buffer );

    free( sequence_buffer->entry_sequence );
    free( sequence_buffer->entry_data );

    memset( sequence_buffer, 0, sizeof( struct reliable_sequence_buffer_t ) );

    free( sequence_buffer );
}

void reliable_sequence_buffer_reset( struct reliable_sequence_buffer_t * sequence_buffer )
{
    assert( sequence_buffer );
    sequence_buffer->sequence = 0;
    memset( sequence_buffer->entry_sequence, 0xFF, sizeof( uint32_t) * sequence_buffer->num_entries );
}

void reliable_sequence_buffer_remove_entries( struct reliable_sequence_buffer_t * sequence_buffer, int start_sequence, int finish_sequence )
{
    assert( sequence_buffer );
    if ( finish_sequence < start_sequence ) 
    {
        finish_sequence += 65535;
    }
    if ( finish_sequence - start_sequence < sequence_buffer->num_entries )
    {
        int sequence;
        for ( sequence = start_sequence; sequence <= finish_sequence; ++sequence )
        {
            sequence_buffer->entry_sequence[ sequence % sequence_buffer->num_entries ] = 0xFFFFFFFF;
        }
    }
    else
    {
        for ( int i = 0; i < sequence_buffer->num_entries; ++i )
        {
            sequence_buffer->entry_sequence[i] = 0xFFFFFFFF;
        }
    }
}

void * reliable_sequence_buffer_insert( struct reliable_sequence_buffer_t * sequence_buffer, uint16_t sequence )
{
    assert( sequence_buffer );
    if ( reliable_sequence_greater_than( sequence + 1, sequence_buffer->sequence ) )
    {
        reliable_sequence_buffer_remove_entries( sequence_buffer, sequence_buffer->sequence, sequence );
        sequence_buffer->sequence = sequence + 1;
    }
    else if ( reliable_sequence_less_than( sequence, sequence_buffer->sequence - sequence_buffer->num_entries ) )
    {
        return NULL;
    }
    int index = sequence % sequence_buffer->num_entries;
    sequence_buffer->entry_sequence[index] = sequence;
    return sequence_buffer->entry_data + index * sequence_buffer->entry_stride;
}

void reliable_sequence_buffer_remove( struct reliable_sequence_buffer_t * sequence_buffer, uint16_t sequence )
{
    assert( sequence_buffer );
    sequence_buffer->entry_sequence[ sequence % sequence_buffer->num_entries ] = 0xFFFFFFFF;
}

int reliable_sequence_buffer_available( struct reliable_sequence_buffer_t * sequence_buffer, uint16_t sequence )
{
    assert( sequence_buffer );
    return sequence_buffer->entry_sequence[ sequence % sequence_buffer->num_entries ] == 0xFFFFFFFF;
}

int reliable_sequence_buffer_exists( struct reliable_sequence_buffer_t * sequence_buffer, uint16_t sequence )
{
    assert( sequence_buffer );
    return sequence_buffer->entry_sequence[ sequence % sequence_buffer->num_entries ] == (uint32_t) sequence;
}

void * reliable_sequence_buffer_find( struct reliable_sequence_buffer_t * sequence_buffer, uint16_t sequence )
{
    assert( sequence_buffer );
    int index = sequence % sequence_buffer->num_entries;
    return ( ( sequence_buffer->entry_sequence[index] == (uint32_t) sequence ) ) ? ( sequence_buffer->entry_data + index * sequence_buffer->entry_stride ) : NULL;
}

void * reliable_sequence_buffer_at_index( struct reliable_sequence_buffer_t * sequence_buffer, int index )
{
    assert( sequence_buffer );
    assert( index >= 0 );
    assert( index < sequence_buffer->num_entries );
    return sequence_buffer->entry_sequence[index] != 0xFFFFFFFF ? ( sequence_buffer->entry_data + index * sequence_buffer->entry_stride ) : NULL;
}

// ---------------------------------------------------------------

struct reliable_endpoint_t
{
    // ...
};

struct reliable_endpoint_t * reliable_endpoint_create( struct reliable_config_t * config )
{
    assert( config );

    (void) config;

    struct reliable_endpoint_t * endpoint = (struct reliable_endpoint_t*) malloc( sizeof( struct reliable_endpoint_t ) );

    // ...

    return endpoint;
}

void reliable_endpoint_destroy( struct reliable_endpoint_t * endpoint )
{
    assert( endpoint );

    // ...

    free( endpoint );
}

// ---------------------------------------------------------------

#if RELIABLE_ENABLE_TESTS

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

static void check_handler( char * condition, 
                           char * function,
                           char * file,
                           int line )
{
    printf( "check failed: ( %s ), function %s, file %s, line %d\n", condition, function, file, line );
#ifndef NDEBUG
    #if defined( __GNUC__ )
        __builtin_trap();
    #elif defined( _MSC_VER )
        __debugbreak();
    #endif
#endif
    exit( 1 );
}

#define check( condition )                                                                      \
do                                                                                              \
{                                                                                               \
    if ( !(condition) )                                                                         \
    {                                                                                           \
        check_handler( #condition, (char*) __FUNCTION__, (char*) __FILE__, __LINE__ );          \
    }                                                                                           \
} while(0)

static void test_endian()
{
    uint32_t value = 0x11223344;

    char * bytes = (char*) &value;

#if RELIABLE_LITTLE_ENDIAN

    check( bytes[0] == 0x44 );
    check( bytes[1] == 0x33 );
    check( bytes[2] == 0x22 );
    check( bytes[3] == 0x11 );

#else // #if RELIABLE_LITTLE_ENDIAN

    check( bytes[3] == 0x44 );
    check( bytes[2] == 0x33 );
    check( bytes[1] == 0x22 );
    check( bytes[0] == 0x11 );

#endif // #if RELIABLE_LITTLE_ENDIAN
}

static void test_sequence_buffer()
{
    // ...
}

#define RUN_TEST( test_function )                                           \
    do                                                                      \
    {                                                                       \
        printf( #test_function "\n" );                                      \
        test_function();                                                    \
    }                                                                       \
    while (0)

void reliable_test()
{
    printf( "\n" );

    //while ( 1 )
    {
        RUN_TEST( test_endian );
        RUN_TEST( test_sequence_buffer );
    }

    printf( "\n*** ALL TESTS PASSED ***\n\n" );
}

#endif // #if RELIABLE_ENABLE_TESTS
