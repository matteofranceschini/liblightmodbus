/*
    liblightmodbus - a lightweight, multiplatform Modbus library
    Copyright (C) 2016  Jacek Wieczorek <mrjjot@gmail.com>

    This file is part of liblightmodbus.

    Liblightmodbus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Liblightmodbus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "lightmodbus/core.h"
#include "lightmodbus/parser.h"
#include "lightmodbus/master/mtypes.h"
#include "lightmodbus/master/minputregisters.h"

uint8_t modbusBuildRequest04( ModbusMaster *status, uint8_t address, uint16_t firstRegister, uint16_t registerCount )
{
	//Build request04 frame, to send it so slave
	//Read multiple input registers

	//Set frame length
	uint8_t frameLength = 8;

	//Set output frame length to 0 (in case of interrupts)
	status->request.length = 0;
	status->finished = 0;

	//Reallocate memory for final frame
	free( status->request.frame );
	status->request.frame = (uint8_t *) malloc( frameLength );
	if ( status->request.frame == NULL )
	{
		status->finished = 1;
		return MODBUS_ERROR_ALLOC;
	}
	union ModbusParser *builder = (union ModbusParser *) status->request.frame;

	builder->base.address = address;
	builder->base.function = 4;
	builder->request04.firstRegister = modbusSwapEndian( firstRegister );
	builder->request04.registerCount = modbusSwapEndian( registerCount );

	//Calculate crc
	builder->request04.crc = modbusCRC( builder->frame, frameLength - 2 );

	status->request.length = frameLength;
	status->finished = 1;

	return 0;
}

uint8_t modbusParseResponse04( ModbusMaster *status, union ModbusParser *parser, union ModbusParser *requestParser )
{
	//Parse slave response to request 04
	//Read multiple input registers

	//Update frame length
	uint8_t frameLength = 5 + parser->response04.byteCount;
	uint8_t dataok = 1;
	uint8_t i = 0;

	//Check frame crc
	if ( modbusCRC( parser->frame, frameLength - 2 ) != parser->response04.values[ parser->response04.byteCount >> 1 ] )
	{
		status->finished = 1;
		return MODBUS_ERROR_CRC;
	}

	//Check between data sent to slave and received from slave
	dataok &= ( parser->response04.address == requestParser->request04.address );
	dataok &= ( parser->response04.function == requestParser->request04.function );
	dataok &= ( parser->response04.byteCount == modbusSwapEndian( requestParser->request04.registerCount ) << 1 );

	//If data is bad abort parsing, and set error flag
	if ( !dataok )
	{
		status->finished = 1;
		return MODBUS_ERROR_FRAME;
	}

	//Allocate memory for ModbusData structures array
	free( status->data );
	status->data = (ModbusData *) malloc( ( parser->response04.byteCount >> 1 ) * sizeof( ModbusData ) );
	if ( status->data == NULL )
	{
		status->finished = 1;
		return MODBUS_ERROR_ALLOC;
	}

	//Copy received data to output structures array
	for ( i = 0; i < ( parser->response04.byteCount >> 1 ); i++ )
	{
		status->data[i].address = parser->base.address;
		status->data[i].dataType = inputRegister;
		status->data[i].reg = modbusSwapEndian( requestParser->request04.firstRegister ) + i;
		status->data[i].value = modbusSwapEndian( parser->response04.values[i] );
	}

	//Set up data length - response successfully parsed
	status->dataLength = parser->response04.byteCount >> 1;
	status->finished = 1;
	return 0;
}
