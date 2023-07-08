#include <net/if.h>
#include <netinet/ip.h>
#include <sched.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
class NixShell {
    std::string dir;
    std::string shell;

    std::string read_env(std::string name) {
        std::ifstream fs(dir + "/env-vars");
        std::string pattern = "declare -x " + name + "=\"";
        std::string line;
        while (std::getline(fs, line)) {
            if (line.find(pattern) == 0) {
                return line.substr(pattern.size(),
                                   line.size() - pattern.size() - 1);
            }
        }
        return "";
    }

    void init_namespace() {
        int ret;
        // clone the user namespace with 'unshare'
        auto current_uid = geteuid();
        auto current_gid = getegid();
        unshare(CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWNET);
        std::fstream fs_setgroups("/proc/self/setgroups", std::ios::out);
        fs_setgroups << "deny";
        fs_setgroups.close();
        // map current uid to 1000
        std::fstream("/proc/self/uid_map", std::ios::out)
            << "1000 " << current_uid << " 1";
        // set gid to 100
        std::fstream("/proc/self/gid_map", std::ios::out)
            << "100 " << current_gid << " 1";
        // set hostname to "localhost"
        ret = sethostname("localhost", 9);
        if (ret != 0) {
            perror("sethostname");
        }
        // set domainname to "(none)"
        ret = setdomainname("(none)", 6);
        if (ret != 0) {
            perror("setdomainname");
        }

        // create a new loopback interface
        struct ifreq ifr;
        strcpy(ifr.ifr_name, "lo");
        ifr.ifr_flags = IFF_UP | IFF_LOOPBACK | IFF_RUNNING;
        auto fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
        ret = ioctl(fd, SIOCSIFFLAGS, &ifr);
        if (ret != 0) {
            perror("ioctl");
        }
        close(fd);
    }

   public:
    NixShell(std::string dir) : dir(dir) {
        shell = read_env("SHELL");
        init_namespace();
    }

    void exec(int argc, const char** argv) {
        std::vector<std::string> args_tmp;
        std::vector<const char*> args;
        args_tmp.push_back(shell.c_str());
        args_tmp.push_back("-c");
        std::string command = "source /build/env-vars; exec ";
        for (int i = 0; i < argc; i++) {
            command += ("\"" + std::string(argv[i]) + "\" ");
        }
        args_tmp.push_back(command);
        for (size_t i = 0; i < args_tmp.size(); i++) {
            args.push_back(args_tmp[i].c_str());
        }
        args.push_back(nullptr);
        // for (size_t i = 0; i < args.size(); i++) {
        //     std::cout << args[i] << "\n";
        // }
        // std::cout << std::endl;
        execvp(shell.c_str(), const_cast<char* const*>(args.data()));
    }
};

int main(int argc, const char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <dir> <command> [args...]"
                  << std::endl;
        return 1;
    }
    NixShell nix(argv[1]);
    nix.exec(argc - 2, argv + 2);
    return 0;
}
