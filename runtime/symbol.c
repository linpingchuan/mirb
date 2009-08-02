#include "symbol.h"
#include "../globals.h"

KHASH_MAP_INIT_STR(symbol, rt_value);

rt_value rt_Symbol;

static khash_t(symbol) *symbols;

void rt_symbols_create(void)
{
	symbols = kh_init(symbol);
}

void rt_symbols_destroy(void)
{
	kh_destroy(symbol, symbols);
}

rt_value rt_symbol_from_cstr(const char* name)
{
	khiter_t k = kh_get(symbol, symbols, name);

	if (k == kh_end(symbols))
	{
		unsigned int name_length = strlen(name);
		char* string =  malloc(name_length + 1);

		strcpy(string, name);

		rt_value symbol = rt_alloc(sizeof(struct rt_symbol));

		RT_COMMON(symbol)->flags = C_SYMBOL;
		RT_COMMON(symbol)->class_of = rt_Symbol;
		RT_SYMBOL(symbol)->string = string;

		int ret;

		k = kh_put(symbol, symbols, string, &ret);

		if(!ret)
		{
			kh_del(symbol, symbols, k);

			printf("Unable to add symbol %s to the list\n", string);
		}

		kh_value(symbols, k) = symbol;

		return symbol;
	}

	return kh_value(symbols, k);
}

