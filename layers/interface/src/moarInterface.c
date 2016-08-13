//
// Created by svalov, kryvashek on 05.07.16.
//

#include <moarInterfacePrivate.h>
#include <moarIfacePhysicsRoutine.h>
#include <moarIfaceChannelRoutine.h>
#include <moarIfaceNeighborsRoutine.h>
#include <moarIfaceTransmitReceive.h>

static IfaceState_T	state = { 0 };

PowerFloat_T calcMinPower( PowerInt_T startPower, PowerFloat_T finishPower ) {
	PowerFloat_T	neededPower = IFACE_MIN_FINISH_POWER + ( PowerFloat_T )startPower - finishPower;

	return ( IFACE_MAX_START_POWER < neededPower ? IFACE_MAX_START_POWER : neededPower );
}

static inline int clearCommand( void ) {
	return FreeCommand( &( state.Memory.Command ) );
}

int processCommandIface( LayerCommandType_T commandType, void * metaData, void * data, size_t dataSize ) {
	static size_t	sizes[ LayerCommandType_TypesCount ] = { 0,	// LayerCommandType_None
																0,	// LayerCommandType_Send
																IFACE_RECEIVE_METADATA_SIZE,	// LayerCommandType_Receive
															  	IFACE_NEIGHBOR_METADATA_SIZE,	// LayerCommandType_NewNeighbor
															  	IFACE_NEIGHBOR_METADATA_SIZE,	// LayerCommandType_LostNeighbor
															  	IFACE_NEIGHBOR_METADATA_SIZE,	// LayerCommandType_UpdateNeighbor
																IFACE_PACK_STATE_METADATA_SIZE,	// LayerCommandType_MessageState
																IFACE_REGISTER_METADATA_SIZE,	// LayerCommandType_RegisterInterface
															  	0,	// LayerCommandType_RegisterInterfaceResult
																IFACE_UNREGISTER_METADATA_SIZE,	// LayerCommandType_UnregisterInterface
																0,	// LayerCommandType_ConnectApplication
																0,	// LayerCommandType_ConnectApplicationResult
																0,	// LayerCommandType_DisconnectApplication
																IFACE_MODE_STATE_METADATA_SIZE,	// LayerCommandType_InterfaceState
																0	// LayerCommandType_UpdateBeaconPayload
															};

	clearCommand();
	state.Memory.Command.Command = commandType;
	state.Memory.Command.MetaSize = sizes[ commandType ];
	state.Memory.Command.MetaData = metaData;
	state.Memory.Command.DataSize = dataSize;
	state.Memory.Command.Data = data;

	return writeUp( &state );
}

int processCommandIfaceRegister( void ) {
	IfaceRegisterMetadata_T	metadata;
	int						result;

	metadata.Length = IFACE_ADDR_SIZE;
	metadata.Value = state.Config.Address;
	result = processCommandIface( LayerCommandType_RegisterInterface, &metadata, NULL, 0 );

	return result;
}

int processCommandChannelRegisterResult( void ) {
	int result;

	state.Config.IsConnectedToChannel = ( ( ChannelRegisterResultMetadata_T * ) state.Memory.Command.MetaData )->Registred;
	clearCommand();

	if( state.Config.IsConnectedToChannel ) {
		printf( "Interface registered in channel layer\n" );
		fflush( stdout );
		result = FUNC_RESULT_SUCCESS;
	} else
		result = processCommandIfaceRegister();

	return result;
}

int processCommandIfaceUnknownDest( void ) {
	IfacePackStateMetadata_T	metadata;

	metadata.Id = state.Memory.ProcessingMessageId;
	metadata.State = IfacePackState_UnknownDest;

	return processCommandIface( LayerCommandType_MessageState, &metadata, NULL, 0 );
}

int processCommandIfaceTimeoutFinished( bool gotResponse ) {
	IfacePackStateMetadata_T	metadata;
	int 						result;

	metadata.Id = state.Memory.ProcessingMessageId;
	metadata.State = ( gotResponse ? IfacePackState_Responsed : IfacePackState_Timeouted );
	result = processCommandIface( LayerCommandType_MessageState, &metadata, NULL, 0 );
	state.Config.IsWaitingForResponse = false;

	if( IFACE_BEACON_INTERVAL > IFACE_RESPONSE_WAIT_INTERVAL )
		state.Config.BeaconIntervalCurrent = IFACE_BEACON_INTERVAL - IFACE_RESPONSE_WAIT_INTERVAL;

	return result;
}

int processCommandIfaceMessageSent( void ) {
	int							result;
	IfacePackStateMetadata_T	metadata;

	metadata.Id = state.Memory.ProcessingMessageId;
	metadata.State = IfacePackState_Sent;
	result = processCommandIface( LayerCommandType_MessageState, &metadata, NULL, 0 );

	return result;
}

int processCommandIfaceNeighborNew( IfaceAddr_T * address ) {
	IfaceNeighborMetadata_T	metadata;

	metadata.Neighbor = *address;

	return processCommandIface( LayerCommandType_NewNeighbor, &metadata, NULL, 0 );
}

int processCommandIfaceNeighborUpdate( IfaceAddr_T * address ) {
	IfaceNeighborMetadata_T	metadata;

	metadata.Neighbor = *address;

	return processCommandIface( LayerCommandType_UpdateNeighbor, &metadata, NULL, 0 );
}

int processReceivedBeacon( IfaceAddr_T * address, PowerFloat_T finishPower ) {
	int				result;
	IfaceNeighbor_T	* sender;
	PowerFloat_T	startPower;

	sender = neighborFind( &state, address );
	startPower = calcMinPower( state.Memory.BufferFooter.MinSensitivity, finishPower );

	if( NULL != sender ) {
		result = neighborUpdate( &state, sender, startPower );

		if( FUNC_RESULT_SUCCESS == result )
			result = processCommandIfaceNeighborUpdate( address );

	} else {
		result = neighborAdd( &state, address, startPower );

		if( FUNC_RESULT_SUCCESS == result )
			result = processCommandIfaceNeighborNew( address );
	}

	return result;
}

int processMockitRegister( void ) {
	struct timeval	moment;
	int				result,
					length,
					address;

	gettimeofday( &moment, NULL );
	srand( ( unsigned int )( moment.tv_usec ) );
	address = 1 + rand() % IFACE_ADDRESS_LIMIT;
	snprintf( state.Memory.Buffer, IFACE_BUFFER_SIZE, "%d%n", address, &length );
	memcpy( &( state.Config.Address ), &address, IFACE_ADDR_SIZE );
	result = writeDown( &state, state.Memory.Buffer, length );

	return result;
}

int processMockitRegisterResult( void ) {
	int	result;

	result = readDown( &state, state.Memory.Buffer, IFACE_BUFFER_SIZE );

	if( 0 >= result )
		return FUNC_RESULT_FAILED_IO;

	state.Config.IsConnectedToMockit = ( 0 == strncmp( IFACE_REGISTRATION_OK, state.Memory.Buffer, IFACE_REGISTRATION_OK_SIZE ) );

	if( state.Config.IsConnectedToMockit ) {
		printf( "Interface registered in MockIT with address %08X\n", *( unsigned int * )&( state.Config.Address ) );
		fflush( stdout );
		return FUNC_RESULT_SUCCESS;
	} else
		return processMockitRegister();
}

int processCommandIfaceReceived( void ) {
	int						result;
	IfaceReceiveMetadata_T	metadata;

	result = midGenerate( &( metadata.Id ), MoarLayer_Interface );

	if( FUNC_RESULT_SUCCESS != result )
		return result;

	metadata.From = state.Memory.BufferHeader.From;
	result = processCommandIface( LayerCommandType_Receive, &metadata, state.Memory.Buffer, state.Memory.BufferHeader.Size );

	return result;
}

int processReceived( void ) {
	int				result;
	IfaceNeighbor_T	* sender;
	IfaceAddr_T		address;
	PowerFloat_T	power;

	if( state.Config.IsConnectedToMockit ) {
		result = receiveAnyData( &state, &power );
		address = state.Memory.BufferHeader.From;

		if( FUNC_RESULT_SUCCESS == result )
			switch( state.Memory.BufferHeader.Type ) {
				case IfacePackType_NeedNoResponse:
					break;

				case IfacePackType_NeedResponse :
					sender = neighborFind( &state, &address );
					result = transmitResponse( &state, sender, state.Memory.BufferHeader.CRC, 0 ); // crc is not implemented yet TODO
					break;

				case IfacePackType_IsResponse :
					// check what message response is for TODO
					result = processCommandIfaceTimeoutFinished( true );
					break;

				case IfacePackType_Beacon :
					result = processReceivedBeacon( &address, power );
					break;

				default :
					result = FUNC_RESULT_FAILED_ARGUMENT;
			}

		if( FUNC_RESULT_SUCCESS == result &&
			IfacePackType_IsResponse != state.Memory.BufferHeader.Type &&
			0 < state.Memory.BufferHeader.Size ) // if contains payload
			result = processCommandIfaceReceived();

	} else
		result = processMockitRegisterResult();

	return result;
}

int connectWithMockit( void ) {
	int	result;

	result = connectDown( &state );

	if( FUNC_RESULT_SUCCESS == result )
		result = processMockitRegister();

	return result;
}

int connectWithMockitAgain( void ) {
	shutdown( state.Config.MockitSocket, SHUT_RDWR );
	close( state.Config.MockitSocket );
	state.Config.IsConnectedToMockit = false;
	return connectDown( &state );
}

int processMockitEvent( uint32_t events ) {
	int result;

	if( events & EPOLLIN ) // if new data received
		result = processReceived();
	else
		result = connectWithMockitAgain();

	return result;
}

int processCommandChannelSend( void ) {
	IfaceNeighbor_T			* neighbor;
	ChannelSendMetadata_T	* metadata;
	int						result;

	metadata = ( ChannelSendMetadata_T * ) state.Memory.Command.MetaData;
	neighbor = neighborFind( &state, &( metadata->To ) );
	state.Memory.ProcessingMessageId = metadata->Id;

	if( NULL == neighbor )
		result = processCommandIfaceUnknownDest();
	else {
		result = transmitMessage( &state, neighbor, metadata->NeedResponse );

		if( FUNC_RESULT_SUCCESS != result )
			return result;

		if( metadata->NeedResponse ) {
			state.Config.BeaconIntervalCurrent = IFACE_RESPONSE_WAIT_INTERVAL;
			state.Config.IsWaitingForResponse = true;
		} else
			result = processCommandIfaceMessageSent();

		clearCommand();
	}

	return result;
}

int processCommandChannelUpdateBeacon( void ) {
	if( ( NULL == state.Memory.Command.Data && 0 < state.Memory.Command.DataSize ) ||
		( NULL != state.Memory.Command.Data && 0 == state.Memory.Command.DataSize ) ||
		IFACE_MAX_PAYLOAD_BEACON_SIZE < state.Memory.Command.DataSize )
		return FUNC_RESULT_FAILED_ARGUMENT;

	memcpy( state.Memory.BeaconPayload, state.Memory.Command.Data, state.Memory.Command.DataSize );
	state.Config.BeaconPayloadSize = state.Memory.Command.DataSize;
	clearCommand();
	return FUNC_RESULT_SUCCESS;
}

int processChannelCommand( void ) {
	int	result;

	result = readUp( &state );

	if( FUNC_RESULT_SUCCESS != result )
		return result;

	if( state.Config.IsConnectedToChannel )
		switch( state.Memory.Command.Command ) {
			case LayerCommandType_Send :
				result = processCommandChannelSend();
				break;

			case LayerCommandType_UpdateBeaconPayload :
				result = processCommandChannelUpdateBeacon();
				break;

			default :
				result = FUNC_RESULT_FAILED_ARGUMENT;
				printf( "IFACE: unknown command %d from channel\n", state.Memory.Command.Command );
		}
	else if( LayerCommandType_RegisterInterfaceResult == state.Memory.Command.Command )
		result = processCommandChannelRegisterResult();

	return result;
}

int connectWithChannel( void ) {
	int	result;

	result = connectUp( &state );

	if( FUNC_RESULT_SUCCESS == result )
		result = processCommandIfaceRegister();

	return result;
}

int connectWithChannelAgain( void ) {
	shutdown( state.Config.ChannelSocket, SHUT_RDWR );
	close( state.Config.ChannelSocket );
	state.Config.IsConnectedToChannel = false;
	return connectUp( &state );
}

int processChannelEvent( uint32_t events ) {
	if( events & EPOLLIN ) // if new command from channel
		return processChannelCommand();
	else
		return connectWithChannelAgain();
}

void * MOAR_LAYER_ENTRY_POINT( void * arg ) {
	struct epoll_event	events[ IFACE_OPENING_SOCKETS ] = {{ 0 }},
						oneSocketEvent;
	int					result,
						epollHandler,
						eventsCount;

	if( NULL == arg )
		return NULL;

	strncpy( state.Config.ChannelSocketFilepath, ( ( MoarIfaceStartupParams_T * )arg )->socketToChannel, SOCKET_FILEPATH_SIZE );

	epollHandler = epoll_create( IFACE_OPENING_SOCKETS );
	oneSocketEvent.events = EPOLLIN | EPOLLET;
	result = connectWithMockit();

	if( FUNC_RESULT_SUCCESS != result )
		return NULL;

	oneSocketEvent.data.fd = state.Config.MockitSocket;
	epoll_ctl( epollHandler, EPOLL_CTL_ADD, state.Config.MockitSocket, &oneSocketEvent );
	result = connectWithChannel();

	if( FUNC_RESULT_SUCCESS != result )
		return NULL;

	oneSocketEvent.data.fd = state.Config.ChannelSocket;
	epoll_ctl( epollHandler, EPOLL_CTL_ADD, state.Config.ChannelSocket, &oneSocketEvent );
	state.Config.BeaconIntervalCurrent = IFACE_BEACON_INTERVAL; // for cases when beaconIntervalCurrent will change

	while( true ) {
		eventsCount = epoll_wait( epollHandler, events, IFACE_OPENING_SOCKETS, state.Config.BeaconIntervalCurrent );
		result = FUNC_RESULT_SUCCESS;

		if( 0 == eventsCount ) {// timeout
			if( state.Config.IsWaitingForResponse )
				result = processCommandIfaceTimeoutFinished( false );
			else
				result = transmitBeacon( &state );
		} else
			for( int eventIndex = 0; eventIndex < eventsCount; eventIndex++ ) {
				if( events[ eventIndex ].data.fd == state.Config.MockitSocket )
					result = processMockitEvent( events[ eventIndex ].events );
				else if( events[ eventIndex ].data.fd == state.Config.ChannelSocket )
					result = processChannelEvent( events[ eventIndex ].events );
				else
					result = FUNC_RESULT_FAILED_ARGUMENT; // wrong socket

				if( FUNC_RESULT_SUCCESS != result ) {
					printf( "IFACE: Error with %d code arised\n", result );
					fflush( stdout );
				}
			}

	}
}