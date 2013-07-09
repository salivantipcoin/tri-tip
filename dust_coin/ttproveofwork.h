
#ifndef TT_NETWORK_H
#define TT_NETWORK_H

namespace  TTcoin
{

	int
	geneSeed( char * _sufix, unsigned _lenght, unsigned _index )
	{
		int id = _lenght + _index - 1;

		if ( id < 0 )
		{
			_sufix[ _lenght ] = 0;
			return _lenght +1;
		}

		char sign = _sufix[ id ];

		if ( sign == 255 )
		{
			_sufix[ id ] = 0;
			return gene( _sufix ,_lenght ,_index - 1 );
		}
		else
		{
			_sufix[ id ]++
		}
		return _lenght;
	}

	char *
	generateProofOfWork( std::string _address, unsigned condition )
	{
		char seed[ 512 ];
		char sha256[ 512 ];
		strcpy ( seed, _address.c_str() );
		unsigned lenght = 0;

		while(1)
		{
			bool  end = true;
			lenght = geneSeed( seed + _address.size(),lenght ,0 );
			simpleSHA256(seed, lenght, sha256 );

				for( int id =0;id < condition; ++id)
				{
					if ( sha256[ id ] != 0 )
					{
						end = false;
						break;
					}
				}

			if ( end == true )
				break;
		}
	}
	return strdup(sha256); 
}



	bool simpleSHA256(unsigned char* input, unsigned long length, unsigned char* md)
	{
		SHA256_CTX context;
		if(!SHA256_Init(&context))
			return false;

		if(!SHA256_Update(&context, input, length))
			return false;

		if(!SHA256_Final(md, &context))
			return false;

		return true;
	}

}
// state machine  to  handle  traffic

#endif
