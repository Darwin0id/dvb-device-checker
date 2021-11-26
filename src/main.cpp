// INCLUDE
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string>
#include <fcntl.h>
#include <dirent.h>
#include <sstream>
#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/net.h>
#include <errno.h>
#include <vector>
#include "json.hpp"

// NAMESPACE
using namespace std;
using namespace nlohmann;
namespace ns {
    struct Card {
        int adapter;
        int device;
        string title;
    };
}

// GLOBALS
static int adapter = 0;
static int device = 0;
static char dev_name[512];
map<int, ns::Card> v_adapters;

// HELPERS
inline std::string trim(std::string& str)
{
    str.erase(str.find_last_not_of(' ')+1);
    str.erase(0, str.find_first_not_of(' '));
    return str;
}

static int get_last_int(const char *str)
{
    int i = 0;
    int i_pos = -1;
    for(; str[i]; ++i)
    {
        const char c = str[i];
        if(c >= '0' && c <= '9')
        {
            if(i_pos == -1)
                i_pos = i;
        }
        else if(i_pos >= 0)
            i_pos = -1;
    }

    if(i_pos == -1)
        return 0;

    return atoi(&str[i_pos]);
}

// RECURSIVE FUNCTION
static void iterate_dir(const char *dir, const char *filter, void (*callback)(const char *))
{
    DIR *dirp = opendir(dir);
    if(!dirp)
    {
        cout << "ERROR: opendir() failed " << dir << strerror(errno);
        return;
    }

    ostringstream item;
    item << dir << "/";
    const int item_len = item.str().length();
    const int filter_len = strlen(filter);

    do
    {
        struct dirent *entry = readdir(dirp);
        if(!entry)
            break;
        if(strncmp(entry->d_name, filter, filter_len))
            continue;
        item.seekp(item_len);
        item << entry->d_name;
        callback(item.str().c_str());
    } while(1);

    closedir(dirp);
}

// CHECK FRONTEND
static void check_device_frontend(void)
{
    ostringstream dev_name;
    dev_name << "/dev/dvb/adapter" << adapter << "/frontend" << device;

    int fd = open(dev_name.str().c_str(), O_RDONLY | O_NONBLOCK);
    if(!fd)
    {
        cout << "ERROR: open() failed " << dev_name.str().c_str() << strerror(errno) << endl;
        return;
    }

    struct dvb_frontend_info feinfo;
    if(ioctl(fd, FE_GET_INFO, &feinfo) < 0)
    {
        cout << "ERROR: FE_GET_INFO failed" << dev_name.str().c_str() << strerror(errno) << endl;
        close(fd);
        return;
    }

    map<int, ns::Card>::iterator it = v_adapters.find(adapter);
    if (it != v_adapters.end()) {
        it->second.title = feinfo.name;
    }

    close(fd);
}

// CHECK DEVICE
static void check_device(const char *item)
{
    device = get_last_int(&item[(sizeof("/dev/dvb/adapter") - 1) + (sizeof("/net") - 1)]);

    ns::Card tmp_card = {adapter, device};
    v_adapters.insert({ adapter, tmp_card});
    check_device_frontend();
}

// CHECK ADAPTER
static void check_adapter(const char *item)
{
    adapter = get_last_int(&item[sizeof("/dev/dvb/adapter") - 1]);
    iterate_dir(item, "net", check_device);
}

// MAIN
int main() {
    iterate_dir("/dev/dvb", "adapter", check_adapter);
    json j_array_of_adapters = json::array();

    for (auto i = v_adapters.begin(); i != v_adapters.end(); ++i) {
        json j;
        j["adapter"] = i->second.adapter;
        j["device"] = i->second.device;
        j["title"] = trim(i->second.title);
        j_array_of_adapters.push_back(j);
    }

    cout << to_string(j_array_of_adapters);
    return 0;
}