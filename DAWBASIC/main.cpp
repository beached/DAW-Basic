// DAWBASIC.cpp : Defines the entry point for the console application.
//

#include "dawbasic.h"

#include <iostream>

int main( int argc, char* argv[] ) {
	daw::basic::Basic b;
	::std::string current_line;
	::std::cout << "DAW BASIC v0.1\nREADY" << ::std::endl;
	while( ::std::getline( ::std::cin, current_line ).good( ) ) {
		if( !b.parse_line( current_line ) ) {
			break;
		}
	}

	system( "PAUSE" );
	return EXIT_SUCCESS;
}
