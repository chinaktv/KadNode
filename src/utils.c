
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>

#include "log.h"
#include "conf.h" /* For gconf->time_now */
#include "sha1.h"
#include "utils.h"


/* Fill buffer with random bytes */
int bytes_random( UCHAR buffer[], size_t size ) {
	int fd;
	int rc;

	fd = open( "/dev/urandom", O_RDONLY );
	if( fd < 0 ) {
		log_err( "Failed to open /dev/urandom" );
		exit( 1 );
	}

	if( (rc = read( fd, buffer, size )) >= 0 ) {
		close( fd );
	}

	return rc;
}

void bytes_from_hex( UCHAR bin[], const char hex[], size_t length ) {
	size_t i;
	size_t xv = 0;

	for( i = 0; i < length; ++i ) {
		const char c = hex[i];
		if( c >= 'a' ) {
			xv += (c - 'a') + 10;
		} else if ( c >= 'A') {
			xv += (c - 'A') + 10;
		} else {
			xv += c - '0';
		}

		if( i % 2 ) {
			bin[i/2] = xv;
			xv = 0;
		} else {
			xv *= 16;
		}
	}
}

char *bytes_to_hex( char hex[], const UCHAR bin[], size_t length ) {
	size_t i;
	UCHAR *p0 = (UCHAR *)bin;
	char *p1 = hex;

	for( i = 0; i < length; i++ ) {
		snprintf( p1, 3, "%02x", *p0 );
		p0 += 1;
		p1 += 2;
	}

	return hex;
}

/*
* Compute the id of a string.
* The character case and the top level domain is ignored.
* If the string represent a sha1 hash in hex - then it is
* converted explicitly instead of hashing.
*/
void id_compute( UCHAR *id, const char *str ) {
	SHA1_CTX ctx;
	size_t size;
	char *cpy;
	char *tld;

	/* Remove the top level domain */
	tld = strrchr( str, '.' );
	if( tld == NULL ) {
		size = strlen( str );
	} else {
		size = tld - str;
	}

	if( size == SHA1_HEX_LENGTH && str_isHex( str, SHA1_HEX_LENGTH ) ) {
		/* Treat hostname as hex string and ignore any kind of suffix */
		bytes_from_hex( id, str, SHA1_HEX_LENGTH );
	} else {
		cpy = strdup( str );
		str_toLower( cpy, size );

		SHA1_Init( &ctx );
		SHA1_Update( &ctx, (const UCHAR *) cpy, size );
		SHA1_Final( &ctx, id );

		free( cpy );
	}
}

int id_equal( const UCHAR *id1, const UCHAR *id2 ) {
	return (memcmp( id1, id2, SHA1_BIN_LENGTH ) == 0);
}

/* Check if string consist of hexdecimal characters */
int str_isHex( const char *string, size_t size ) {
	size_t i = 0;

	for( i = 0; i < size; i++ ) {
		const char c = string[i];
		if( (c >= '0' && c <= '9')
			|| (c >= 'A' && c <= 'F')
			|| (c >= 'a' && c <= 'f') ) {
			continue;
		} else {
			return 0;
		}
	}

	return 1;
}

int str_isValidHostname( const char *hostname, size_t size ) {
	size_t i;

	for( i = 0; i < size; i++ ) {
		const char c = hostname[i];
		if( (c >= '0' && c <= '9')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= 'a' && c <= 'z')
			|| (c == '-')
			|| (c == '.')
			|| (c == '_') ) {
			continue;
		} else {
			return 0;
		}
	}

	return 1;
}

int str_isZero( const char* str ) {
	return (str == NULL) || (strcmp( str, "0" ) == 0);
}

void str_toLower( char* str, size_t size ) {
	size_t i;

	for( i = 0; i < size; ++i ) {
		str[i] = tolower( str[i] );
	}
}

char *str_id( const UCHAR *in, char *buf ) {
	return bytes_to_hex( buf, in, SHA1_BIN_LENGTH );
}

char *str_addr( const IP *addr, char *addrbuf ) {
	char buf[INET6_ADDRSTRLEN+1];
	unsigned short port;

	switch( addr->ss_family ) {
		case AF_INET6:
			port = ntohs( ((IP6 *)addr)->sin6_port );
			inet_ntop( AF_INET6, &((IP6 *)addr)->sin6_addr, buf, sizeof(buf) );
			sprintf( addrbuf, "[%s]:%hu", buf, port );
			break;
		case AF_INET:
			port = ntohs( ((IP4 *)addr)->sin_port );
			inet_ntop( AF_INET, &((IP4 *)addr)->sin_addr, buf, sizeof(buf) );
			sprintf( addrbuf, "%s:%hu", buf, port );
			break;
		default:
			sprintf( addrbuf, "<invalid address>" );
	}

	return addrbuf;
}

char *str_addr6( const IP6 *addr, char *addrbuf ) {
	return str_addr( (const IP *)addr, addrbuf );
}

char *str_addr4( const IP4 *addr, char *addrbuf ) {
	return str_addr( (const IP *)addr, addrbuf );
}

int addr_port( const IP *addr ) {
	switch( addr->ss_family ) {
		case AF_INET: return ntohs( ((IP4 *)addr)->sin_port );
		case AF_INET6: return ntohs( ((IP6 *)addr)->sin6_port );
		default: return 0;
	}
}

int addr_len( const IP *addr ) {
	switch( addr->ss_family ) {
		case AF_INET: return sizeof(IP4);
		case AF_INET6: return sizeof(IP6);
		default: return 0;
	}
}

/*
* Parse/Resolve an IP address.
* The port must be specified separately.
*/
int addr_parse( IP *addr, const char *addr_str, const char *port_str, int af ) {
	struct addrinfo hints;
	struct addrinfo *info = NULL;
	struct addrinfo *p = NULL;

	memset( &hints, '\0', sizeof(struct addrinfo) );
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = af;

	if( getaddrinfo( addr_str, port_str, &hints, &info ) != 0 ) {
		return ADDR_PARSE_CANNOT_RESOLVE;
	}

	p = info;
	while( p != NULL ) {
		if( p->ai_family == AF_INET6 ) {
			memcpy( addr, p->ai_addr, sizeof(IP6) );
			freeaddrinfo( info );
			return ADDR_PARSE_SUCCESS;
		}
		if( p->ai_family == AF_INET ) {
			memcpy( addr, p->ai_addr, sizeof(IP4) );
			freeaddrinfo( info );
			return ADDR_PARSE_SUCCESS;
		}
	}

	freeaddrinfo( info );
	return ADDR_PARSE_NO_ADDR_FOUND;
}

/*
* Parse/Resolve various string representations of
* IPv4/IPv6 addresses and optional port.
* An address can also be a domain name.
* A port can also be a service  (e.g. 'www').
*
* "<address>"
* "<ipv4_address>:<port>"
* "[<address>]"
* "[<address>]:<port>"
*/
int addr_parse_full( IP *addr, const char *full_addr_str, const char* default_port, int af ) {
	char addr_buf[256];

	char *addr_beg, *addr_tmp;
	char *last_colon;
	const char *addr_str = NULL;
	const char *port_str = NULL;
	size_t len;

	len = strlen( full_addr_str );
	if( len >= (sizeof(addr_buf) - 1) ) {
		/* address too long */
		return ADDR_PARSE_INVALID_FORMAT;
	} else {
		addr_beg = addr_buf;
	}

	memset( addr_buf, '\0', sizeof(addr_buf) );
	memcpy( addr_buf, full_addr_str, len );

	last_colon = strrchr( addr_buf, ':' );

	if( addr_beg[0] == '[' ) {
		/* [<addr>] or [<addr>]:<port> */
		addr_tmp = strrchr( addr_beg, ']' );

		if( addr_tmp == NULL ) {
			/* broken format */
			return ADDR_PARSE_INVALID_FORMAT;
		}

		*addr_tmp = '\0';
		addr_str = addr_beg + 1;

		if( *(addr_tmp+1) == '\0' ) {
			port_str = default_port;
		} else if( *(addr_tmp+1) == ':' ) {
			port_str = addr_tmp + 2;
		} else {
			/* port expected */
			return ADDR_PARSE_INVALID_FORMAT;
		}
	} else if( last_colon && last_colon == strchr( addr_buf, ':' ) ) {
		/* <non-ipv6-addr>:<port> */
		addr_tmp = last_colon;
		if( addr_tmp ) {
			*addr_tmp = '\0';
			addr_str = addr_buf;
			port_str = addr_tmp+1;
		} else {
			addr_str = addr_buf;
			port_str = default_port;
		}
	} else {
		/* <addr> */
		addr_str = addr_buf;
		port_str = default_port;
	}

	return addr_parse( addr, addr_str, port_str, af );
}

/* Compare two ip addresses, ignore port */
int addr_equal( const IP *addr1, const IP *addr2 ) {
	if( addr1->ss_family != addr2->ss_family ) {
		return 0;
	} else if( addr1->ss_family == AF_INET ) {
		return memcmp( &((IP4 *)addr1)->sin_addr, &((IP4 *)addr2)->sin_addr, 4 ) == 0;
	} else if( addr1->ss_family == AF_INET6 ) {
		return memcmp( &((IP6 *)addr1)->sin6_addr, &((IP6 *)addr2)->sin6_addr, 16 ) == 0;
	} else {
		return 0;
	}
}

time_t time_now_sec( void ) {
	return gconf->time_now.tv_sec;
}

time_t time_add_min( unsigned int min ) {
	return time_now_sec() + (60 * min);
}
