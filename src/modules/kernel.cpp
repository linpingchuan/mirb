#include "kernel.hpp"
#include "../classes/object.hpp"
#include "../classes/string.hpp"
#include "../classes/symbol.hpp"
#include "../classes/array.hpp"
#include "../classes/exception.hpp"
#include "../classes/exceptions.hpp"
#include "../classes/file.hpp"
#include "../platform/platform.hpp"
#include "../char-array.hpp"
#include "../runtime.hpp"

namespace Mirb
{
	value_t Kernel::at_exit(value_t block)
	{
		if(type_error(block, context->proc_class))
			return 0;

		context->at_exits.push(block);

		return block;
	}
	
	value_t Kernel::proc(value_t block)
	{
		return block;
	}
	
	value_t Kernel::benchmark(value_t block)
	{
		CharArray result;
		
		OnStackString<1> os(result);

		bool exception = false;

		result = Mirb::Platform::benchmark([&] {
			if(!yield(block))
				exception = true;
		}).format();

		return exception ? 0 : result.to_string();
	}
	
	value_t Kernel::eval(value_t obj, value_t code)
	{
		if(Value::type(code) != Value::String)
			return value_nil;

		String *input = cast<String>(code);

		CharArray c_str = input->string.c_str();
		CharArray filename("(eval)");

		return eval(obj, Symbol::from_literal("in eval"), current_frame->prev->scope, c_str.str_ref(), c_str.str_length(), filename);
	}

	FILE *try_file(const CharArray &filename, CharArray &result)
	{
		CharArray filename_c_str = filename.c_str();

		#ifndef WIN32
			struct stat buf;
		#endif

		bool is_dir = false;
		FILE* file = 0;

		#ifndef WIN32
			if(stat(filename_c_str.c_str_ref(), &buf) != -1)
				is_dir = S_ISDIR(buf.st_mode);
		#endif

		if(!is_dir)
			file = fopen(filename_c_str.c_str_ref(), "rb");

		if(is_dir || !file)
		{
			is_dir = false;
			
			CharArray filename_rb = filename + ".rb";
			CharArray filename_rb_cstr = filename_rb.c_str();
			
			#ifndef WIN32
				if(stat(filename_rb_cstr.c_str_ref(), &buf) != -1)
					is_dir = S_ISDIR(buf.st_mode);
			#endif

			if(!is_dir)
				file = fopen(filename_rb_cstr.c_str_ref(), "rb");

			if(is_dir || !file)
				return 0;

			result = filename_rb;

			return file;
		}

		return file;
	}
	
	FILE *open_file(CharArray &filename, bool try_relative)
	{
		auto load_path = cast<Array>(get_global(Symbol::get("$:")));
		FILE *result = nullptr;
		
		load_path->vector.each([&](value_t path) -> bool {
			auto str = cast<String>(path)->string;

			JoinSegments joiner;
			
			joiner.push(str);
			joiner.push(filename);

			str = joiner.join();

			result = try_file(str, filename);

			return result == nullptr;
		});

		if(result)
			return result;

		if(try_relative)
			return try_file(filename, filename);
		else
			return nullptr;
	}

	value_t run_file(value_t self, CharArray filename, bool try_relative, bool require)
	{
		FILE* file = open_file(filename, try_relative);

		if(!file)
			return raise(context->load_error, "Unable to find file '" + filename + "'");

		CharArray full_path = File::expand_path(filename);

		if(require && !context->loaded.each([&](value_t path) { return cast<String>(path)->string != full_path;	}))
		{
			return value_nil;
		}

		context->loaded.push(full_path.to_string());

		fseek(file, 0, SEEK_END);

		size_t length = ftell(file);

		fseek(file, 0, SEEK_SET);

		char *data = (char *)malloc(length + 1);

		if(!data)
			return raise(context->load_error, "Unable to allocate memory for file content '" + filename + "' (" + CharArray::uint(length) + " bytes)");

		mirb_runtime_assert(data != 0);

		if(fread(data, 1, length, file) != length)
		{
			free(data);
			fclose(file);

			return raise(context->load_error, "Unable to read content of file '" + filename + "'");
		}

		data[length] = 0;

		value_t result = eval(self, Symbol::get("main"), context->object_scope, (char_t *)data, length, filename);

		free(data);
		fclose(file);

		return result == 0 ? 0 : value_nil;
	}
	
	value_t Kernel::load(value_t filename)
	{
		return run_file(context->main, cast<String>(filename)->string, true, false);
	}
	
	value_t Kernel::require(value_t filename)
	{
		return run_file(context->main, cast<String>(filename)->string, false, true);
	}
	
	value_t Kernel::require_relative(value_t filename)
	{
		return run_file(context->main, cast<String>(filename)->string, true, true);
	}
	
	value_t Kernel::print(size_t argc, value_t argv[])
	{
		for(size_t i = 0; i < argc; ++i)
		{
			value_t arg = argv[i];

			if(Value::type(arg) != Value::String)
				arg = call(arg, "to_s");

			if(!arg)
				return 0;
			
			if(Value::type(arg) == Value::String)
				std::cout << cast<String>(arg)->string.get_string();
		}
		
		return value_nil;
	}
	
	value_t Kernel::puts(size_t argc, value_t argv[])
	{
		for(size_t i = 0; i < argc; ++i)
		{
			value_t arg = argv[i];

			if(Value::type(arg) != Value::String)
				arg = call(arg, "to_s");
			
			if(!arg)
				return 0;
			
			if(Value::type(arg) == Value::String)
				std::cout << cast<String>(arg)->string.get_string();

			std::cout << "\n";
		}
		
		return value_nil;
	}
	
	value_t Kernel::raise(size_t argc, value_t argv[])
	{
		Class *instance_of;
		value_t message;

		size_t i = 0;

		if((argc > i) && (Value::type(argv[i]) == Value::Class))
		{
			instance_of = auto_cast(argv[i]);
			i++;
		}
		else
			instance_of = context->runtime_error;
	
		if(argc > i)
		{
			message = argv[i];
			i++;
		}
		else
			message = value_nil;

		Exception *exception = Collector::allocate<Exception>(instance_of, message, Mirb::backtrace());

		return raise(exception);
	}

	value_t Kernel::backtrace()
	{
		return StackFrame::get_backtrace(Mirb::backtrace()).to_string();
	}

	void Kernel::initialize()
	{
		context->kernel_module = define_module("Kernel");

		include_module(context->object_class, context->kernel_module);
		
		method<Arg::Block>(context->kernel_module, "at_exit", &at_exit);
		method<Arg::Block>(context->kernel_module, "proc", &proc);
		method<Arg::Block>(context->kernel_module, "benchmark", &benchmark);
		method(context->kernel_module, "backtrace", &backtrace);
		method<Arg::Self, Arg::Value>(context->kernel_module, "eval", &eval);
		method<Arg::Count, Arg::Values>(context->kernel_module, "print", &print);
		method<Arg::Count, Arg::Values>(context->kernel_module, "puts", &puts);
		method<Arg::Value>(context->kernel_module, "load", &load);
		method<Arg::Value>(context->kernel_module, "require", &require);
		method<Arg::Value>(context->kernel_module, "require_relative", &require_relative);
		method<Arg::Count, Arg::Values>(context->kernel_module, "raise", &raise);
	}
};
