#include <typeinfo>
#include <string>

using namespace std;

int initRTTI()
{
	int r;
	r += typeid(void).name()[0];
	r += typeid(void*).name()[0];
	r += typeid(const void*).name()[0];
	r += typeid(bool).name()[0];
	r += typeid(int).name()[0];
	r += typeid(unsigned).name()[0];
	r += typeid(int*).name()[0];
	r += typeid(const int*).name()[0];
	r += typeid(short).name()[0];
	r += typeid(unsigned short).name()[0];
	r += typeid(long).name()[0];
	r += typeid(unsigned long).name()[0];
    r += typeid(char).name()[0];
    r += typeid(unsigned char).name()[0];
    r += typeid(char*).name()[0];
    r += typeid(unsigned char*).name()[0];
    r += typeid(const char*).name()[0];
    r += typeid(const unsigned char*).name()[0];
	r += typeid(float).name()[0];
	r += typeid(double).name()[0];
	return r;
}
