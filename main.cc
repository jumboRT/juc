#include "converter.hh"
#include <boost/program_options.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

void print_usage(const std::string &name) {
        std::cerr << name << " <model>" << std::endl;
}

int main(int argc, char *argv[]) {
        namespace po = boost::program_options;
        namespace fs = std::filesystem;
        po::options_description desc("options");
        po::positional_options_description pdesc;

        // add option to add custom header
        // add option to flip triangulation
        // check if input file exists, because assimp doesn't check that
        desc.add_options()("help,h", "produce a help message")(
            "input-file,i", po::value<fs::path>(),
            "specify the model to convert")(
            "output-file,o", po::value<std::string>(),
            "specify the file to put the output in")(
            "name,n", po::value<std::string>(),
            "specify the name to give to the converted scene file")(
            "smooth,-s", "generate smooth normals");

        pdesc.add("input-file", -1);

        po::variables_map vm;
        try {
                po::store(po::command_line_parser(argc, argv)
                              .options(desc)
                              .positional(pdesc)
                              .run(),
                          vm);
        } catch (const std::exception &ex) {
                std::cerr << argv[0] << ": " << ex.what() << std::endl;
                return EXIT_FAILURE;
        }

        po::notify(vm);
        if (vm.count("help")) {
                std::cout << desc << std::endl;
                return EXIT_SUCCESS;
        }
        if (vm.count("input-file") == 0) {
                std::cerr << argv[0] << ": no input file specified"
                          << std::endl;
                return EXIT_FAILURE;
        }
        const fs::path in_file = vm["input-file"].as<fs::path>();
        if (!fs::exists(in_file)) {
                std::cerr << argv[0] << ": " << in_file.string()
                          << ": does not exist" << std::endl;
                return EXIT_FAILURE;
        } else if (fs::status(in_file).type() == fs::file_type::directory) {
                std::cerr << argv[0] << ": " << in_file.string()
                          << ": is a directory" << std::endl;
                return EXIT_FAILURE;
        }
        std::string name = in_file.stem();
        if (vm.count("name")) {
                name = vm["name"].as<std::string>();
        }
        try {
                if (vm.count("output-file")) {
                        std::fstream out_file
                            = std::fstream(vm["output-file"].as<std::string>(),
                                           std::ios::out);
                        converter conv(in_file.string(), out_file, name,
                                       vm.count("smooth") != 0);
                        conv.convert();
                } else {
                        converter conv(in_file.string(), std::cout, name,
                                       vm.count("smooth") != 0);
                        conv.convert();
                }
        } catch (const std::exception &ex) {
                std::cout << argv[0] << ": " << ex.what() << std::endl;
                return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
}
