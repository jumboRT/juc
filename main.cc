#include <cstdlib>
#include <iostream>
#include "converter.hh"

void print_usage(const std::string &name) {
	std::cerr << name << " <model>" << std::endl;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		print_usage(argv[0]);
		return EXIT_SUCCESS;
	}
	const std::string file(argv[1]);
	try {
		texture_converter conv(file, file + ".bmp");
		conv.convert();
	} catch (const std::exception &ex) {
		std::cout << argv[0] << ": " << ex.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
