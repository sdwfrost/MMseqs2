#include "FileUtil.h"
#include "Parameters.h"
#include "DBReader.h"
#include <cstdlib>

int rmdb(int argc, const char **argv, const Command &command) {
    Parameters &par = Parameters::getInstance();
    par.parseParameters(argc, argv, command, 1);
    DBReader<unsigned int>::removeDb(par.db1);
    return EXIT_SUCCESS;
}
