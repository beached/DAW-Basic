#ifndef HAS_STATEMENT_HEADER
#define HAS_STATEMENT_HEADER

#include <string>
#include <memory>

namespace daw {
	namespace basic {
		class Basic;

		class BasicStatement { 
		public:
			virtual ~BasicStatement( ) = 0;
			virtual bool can_parse( const std::string& ) = 0;
			virtual bool parse( Basic& basic, std::string ) = 0;
		};
	}
}

#endif	//HAS_STATEMENT_HEADER
