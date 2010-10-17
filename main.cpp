#include "globals.hpp"
#include "runtime/classes.hpp"
#include "runtime/classes/symbol.hpp"
#include "runtime/classes/string.hpp"
#include "compiler/generator/x86.hpp"

#include <iostream>
#include "src/compiler.hpp"
#include "src/bytecode/generator.hpp"

#ifdef DEBUG
	#include "src/tree/printer.hpp"
#endif

using namespace Mirb;

int main()
{
	#ifndef VALGRIND
		GC_INIT();
	#endif
	
	rt_create();
	
	while(1)
	{
		std::string line;
		
		std::getline(std::cin, line);
		
		Compiler compiler;
		
		compiler.filename = "Input";
		compiler.load((const char_t *)line.c_str(), line.length());
		
		if(compiler.parser.lexeme() == Lexeme::END)
			break;
		
		Scope scope;
		
		compiler.parser.parse_main(scope);
		
		if(!compiler.messages.empty())
		{
			for(auto i = compiler.messages.begin(); i; ++i)
				std::cout << i().format() << "\n";
				
			continue;
		}
	
		#ifdef DEBUG
			DebugPrinter printer;
			
			std::cout << "Parsing done.\n-----\n";
			std::cout << printer.print_node(scope.group);
			std::cout << "\n-----\n";
		#endif
		
		ByteCodeGenerator generator(compiler);
		
		struct block *block = generator.to_bytecode(scope);
		
		if(!compiler.messages.empty())
		{
			for(auto i = compiler.messages.begin(); i; ++i)
				std::cout << i().format() << "\n";
			
			continue;
		}
		
		struct rt_block *runtime_block = compile_block(block);
		
		#ifdef DEBUG
			printf("Running block %x: ", (rt_value)runtime_block);
		#endif
		
		//__asm__("int3\n"); // Make debugging life easier
		
		rt_value result = runtime_block->compiled(0, RT_NIL, rt_class_of(rt_main), rt_main, RT_NIL, 0, 0);
		
		printf("=> "); rt_print(result); printf("\n");
		
		runtime_block = 0; // Make sure runtime_block stays on stack*/
	}
	
	std::cout << "Exiting gracefully...";
	
	rt_destroy();
	
	return 0;
}
