
#ifndef _KAD_H_
#define _KAD_H_


/* Setup the DHT */
void kad_setup( void );

/* Ping this node for bootstrapping */
int kad_ping( const IP *addr );

/* Blacklist a specific address */
int kad_blacklist( const IP* addr );

/*
* Lookup the address of the node whose node id matches id.
* The lookup will be performed on the results of kad_lookup_value().
*/
int kad_lookup_node( const char query[], IP *addr_return );

/*
* Lookup the addresses of the nodes who have announced value id.
* The first call will start the search.
*/
int kad_lookup_value( const char query[], IP addr_array[], size_t *addr_num );

/* Export good nodes */
int kad_export_nodes( IP addr_array[], size_t *addr_num );

/* Print status information */
int kad_status( char *buf, int len );

/* Count all nodes in the given bucket */
int kad_count_nodes( void );

/* 
* Announce that the resource identified by id can
* be served by this computer using the given port.
*/
int kad_announce( const UCHAR *id, int port );

/* Various debug functions */
void kad_debug_buckets( int fd );
void kad_debug_searches( int fd );
void kad_debug_storage( int fd );
void kad_debug_blacklist( int fd );
void kad_debug_constants( int fd );

#endif /* _KAD_H_ */
