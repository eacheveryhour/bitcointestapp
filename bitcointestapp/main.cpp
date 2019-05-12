#include <iostream>
#include <getopt.h>
#include <processor.h>
#include <stdlib.h>

using namespace jsonrpc;
using namespace std;

static struct option long_options[] =
    {
        {"user", required_argument, nullptr, 'u'},
        {"pass", required_argument, nullptr, 'p'},
        {"key", required_argument, nullptr, 'k'},
        {"db", required_argument, nullptr, 'd'},
        {"log", required_argument, nullptr, 'l'},
        {"regtest", no_argument, nullptr, 'r'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

static void print_usage()
{
    printf("Usage: test (-u|-user <RpcConnectionLogin>) (-p|-pass <RpcConnectionPassword>) (-d|-db <DatabaseLocation>) (-l|-log <LogVerbosity [0-6]>)(-k|-key <XpubKey>) (-r[--regtest]) \n\n");
    printf("After executing the daemon, go to /tmp/, cat testpipeout, and echo commands to testpipein. \n");
    printf("Available commands are: GenerateAddress (this will generate new Bitcoin address from passed XPUB on daemon start), \"GetBalance Address\" (this will return address balance from DB, if it was already updated! Quotes needed!) \n\n");
    printf("Examples: \n");
    printf("echo \"GenerateAddress\" > testpipein \n");
    printf("echo \"GetBalance 16ftSEQ4ctQFDtVZiUBusQUjRrGhM3JYwe\" > testpipein \n");

}

int main(int argc, char* argv[])
{
    if(argc == 1)
    {
        print_usage();
        exit(EXIT_FAILURE);
    }

    /* start ECC context */
    btc_ecc_start();

    int long_index = 0;
    int opt = 0;

    StartUpParameters parameters;

//    parameters.DatabaseLocation = "/tmp/";
//    parameters.CurlEndpoint = "http://127.0.0.1:8332";
//    parameters.RpcLogin = "ivan";
//    parameters.RpcPassword = "1234";
//    parameters.XpubAddress = "xpub6CUGRUonZSQ4TWtTMmzXdrXDtypWKiKrhko4egpiMZbpiaQL2jkwSB1icqYh2cfDfVxdx4df189oLKnC5fSwqPfgyP3hooxujYzAu3fDVmz";

    /* get arguments */
    while ((opt = getopt_long_only(argc, argv, "u:p:k:d:r", long_options, &long_index)) != -1)
    {
        switch (opt) {
        case 'h':
            print_usage();
            break;
        case 'u':
            parameters.RpcLogin = optarg;
            break;
        case 'p':
            parameters.RpcPassword = optarg;
            break;
        case 'k':
            parameters.XpubAddress = optarg;
            break;
        case 'd':
            parameters.DatabaseLocation = optarg;
            break;
        case 'r':
            parameters.IsRegtest = true;
            break;
        case 'l':
            ConfigureLoggerSeverity((plog::Severity)atoi(optarg));
            break;
        default:
            print_usage();
            exit(EXIT_FAILURE);
        }
    }

    if(parameters.RpcLogin.size() == 0 || parameters.RpcPassword.size() == 0)
    {
        printf("No PRC login or password, exitting.");
        exit(EXIT_FAILURE);
    }

    Processor varname(parameters);

    btc_ecc_stop();
    return 0;
}
