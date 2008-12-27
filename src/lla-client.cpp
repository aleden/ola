/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  lla-client.cpp
 *  The multi purpose lla client.
 *  Copyright (C) 2005-2008 Simon Newton
 */

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <getopt.h>

#include <string>
#include <vector>

#include <lla/SimpleClient.h>
#include <lla/LlaClient.h>
#include <lla/select_server/SelectServer.h>

using lla::LlaPlugin;
using lla::LlaUniverse;
using lla::LlaDevice;
using lla::LlaPort;
using lla::SimpleClient;
using lla::LlaClient;
using lla::select_server::SelectServer;


/*
 * The mode is determined by the name in which we were called
 */
typedef enum {
  DEVICE_INFO,
  DEVICE_PATCH,
  PLUGIN_INFO,
  UNIVERSE_INFO,
  UNIVERSE_NAME,
  UNI_MERGE,
  SET_DMX,
} mode;


typedef struct {
  mode m;          // mode
  int uni;         // universe id
  int plugin_id;   // plugin id
  int help;        // show the help
  int device_id;   // device id
  int port_id;     // port id
  lla::PatchAction patch_action;      // patch or unpatch
  LlaUniverse::merge_mode merge_mode; // the merge mode
  string cmd;      // argv[0]
  string uni_name; // universe name
  string dmx;      // dmx string
} options;


/*
 * The observer class which repsonds to events
 */
class Observer: public lla::LlaClientObserver {
  public:
    Observer(options *opts, SelectServer *ss) : m_opts(opts), m_ss(ss) {}

    void Plugins(const vector <LlaPlugin> &plugins, const string &error);
    void Devices(const vector <LlaDevice> devices, const string &error);
    void Universes(const vector <LlaUniverse> universes, const string &error);
    void PatchComplete(const string &error);
    void UniverseNameComplete(const string &error);
    void UniverseMergeModeComplete(const string &error);

  private:
    options *m_opts;
    SelectServer *m_ss;
};


/*
 * This is called when we recieve universe results from the client
 * @param universes a vector of LlaUniverses
 */
void Observer::Universes(const vector <LlaUniverse> universes, const string &error) {
  vector<LlaUniverse>::const_iterator iter;

  if (!error.empty()) {
    printf("%s\n", error.c_str());
    m_ss->Terminate();
    return;
  }

  printf("   ID\t%30s\t\tMerge Mode\n", "Name");
  printf("----------------------------------------------------------\n");

  for(iter = universes.begin(); iter != universes.end(); ++iter) {
    printf("%5d\t%30s\t\t%s\n", iter->Id(), iter->Name().c_str(),
           iter->MergeMode() == LlaUniverse::MERGE_HTP ? "HTP" : "LTP") ;
  }
  printf("----------------------------------------------------------\n");
  m_ss->Terminate();
}


/*
 * @params plugins a vector of LlaPlugins
 */
void Observer::Plugins(const vector <LlaPlugin> &plugins, const string &error) {
  vector<LlaPlugin>::const_iterator iter;

  if (!error.empty()) {
    printf("%s\n", error.c_str());
    m_ss->Terminate();
    return;
  }

  if (m_opts->plugin_id > 0 && m_opts->plugin_id < LLA_PLUGIN_LAST) {
    for(iter = plugins.begin(); iter != plugins.end(); ++iter) {
      if(iter->Id() == m_opts->plugin_id)
        printf("%s\n", iter->Description().c_str());
    }
  } else {
    printf("   ID\tDevice Name\n");
    printf("--------------------------------------\n");

    for(iter = plugins.begin(); iter != plugins.end(); ++iter) {
      printf("%5d\t%s\n", iter->Id(), iter->Name().c_str()) ;
    }
    printf("--------------------------------------\n");
  }
  m_ss->Terminate();
}


/*
 * @param devices a vector of LlaDevices
 */
void Observer::Devices(const vector <LlaDevice> devices, const string &error) {
  vector<LlaDevice>::const_iterator iter;

  if (!error.empty()) {
    printf("%s\n", error.c_str());
    m_ss->Terminate();
    return;
  }

  for (iter = devices.begin(); iter != devices.end(); ++iter) {
    printf("Device %d: %s\n", iter->Id(), iter->Name().c_str());
    vector<LlaPort> ports = iter->Ports();
    vector<LlaPort>::const_iterator port_iter;

    for (port_iter = ports.begin(); port_iter != ports.end(); ++port_iter) {
      printf("  port %d, cap ", port_iter->Id());

      if (port_iter->Capability() == LlaPort::LLA_PORT_CAP_IN)
        printf("IN");
      else
        printf("OUT");

      if (port_iter->IsActive())
        printf(", universe %d", port_iter->Universe()) ;
      printf("\n");
    }
  }
  m_ss->Terminate();
}


/*
 * Called when the patch command completes.
 */
void Observer::PatchComplete(const string &error) {

  if (!error.empty())
    printf("%s\n", error.c_str());
  m_ss->Terminate();
}

/*
 * Called when the name command completes.
 */
void Observer::UniverseNameComplete(const string &error) {
  if (!error.empty())
    printf("%s\n", error.c_str());
  m_ss->Terminate();
}


void Observer::UniverseMergeModeComplete(const string &error) {
  if (!error.empty())
    printf("%s\n", error.c_str());
  m_ss->Terminate();
}


/*
 * Init options
 */
void InitOptions(options *opts) {
  opts->m = DEVICE_INFO;
  opts->uni = -1;
  opts->plugin_id = -1 ;
  opts->help = 0 ;
  opts->patch_action = lla::PATCH;
  opts->port_id = -1;
  opts->device_id = -1;
  opts->merge_mode = LlaUniverse::MERGE_HTP;
}


/*
 * Decide what mode we're running in
 */
void SetMode(options *opts) {
  string::size_type pos = opts->cmd.find_last_of("/");

  if (pos != string::npos)
    opts->cmd = opts->cmd.substr(pos + 1);

  if (opts->cmd == "lla_plugin_info")
    opts->m = PLUGIN_INFO;
  else if (opts->cmd == "lla_patch")
    opts->m = DEVICE_PATCH;
  else if (opts->cmd == "lla_uni_info")
    opts->m = UNIVERSE_INFO;
  else if (opts->cmd == "lla_uni_name")
    opts->m = UNIVERSE_NAME;
  else if (opts->cmd == "lla_uni_merge")
    opts->m = UNI_MERGE;
  else if (opts->cmd == "lla_set_dmx")
    opts->m = SET_DMX;
}


/*
 * parse our cmd line options
 *
 */
void ParseOptions(int argc, char *argv[], options *opts) {
  static struct option long_options[] = {
      {"plugin_id", required_argument, 0, 'p'},
      {"help", no_argument, 0, 'h'},
      {"ltp", no_argument, 0, 'l'},
      {"name", required_argument, 0, 'n'},
      {"universe", required_argument, 0, 'u'},
      {"dmx", required_argument, 0, 'd'},
      {0, 0, 0, 0}
    };

  int c;
  int option_index = 0;

  while (1) {
    c = getopt_long(argc, argv, "ld:n:u:p:hv", long_options, &option_index);

    if (c == -1)
      break;

    switch (c) {
      case 0:
        break;
      case 'p':
        opts->plugin_id = atoi(optarg);
        break;
      case 'h':
        opts->help = 1;
        break;
      case 'l':
        opts->merge_mode = LlaUniverse::MERGE_LTP;
        break;
      case 'n':
        opts->uni_name = optarg;
        break;
      case 'u':
        opts->uni = atoi(optarg);
        break;
      case 'd':
        opts->dmx = optarg;
            break;
      case '?':
        break;
      default:
        ;
    }
  }
}


/*
 * parse our cmd line options for the patch command
 */
int ParsePatchOptions(int argc, char *argv[], options *opts) {
  static struct option long_options[] = {
      {"patch", no_argument, 0, 'a'},
      {"unpatch", no_argument, 0, 'r'},
      {"device", required_argument, 0, 'd'},
      {"port", required_argument, 0, 'p'},
      {"universe", required_argument, 0, 'u'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}
    };

  int c;
  int option_index = 0;

  while (1) {
    c = getopt_long(argc, argv, "ard:p:u:h", long_options, &option_index);

    if (c == -1)
      break;

    switch (c) {
      case 0:
        break;
      case 'a':
        opts->patch_action = lla::PATCH;
            break;
      case 'd':
        opts->device_id = atoi(optarg);
        break;
      case 'p':
        opts->port_id = atoi(optarg);
        break;
      case 'r':
        opts->patch_action = lla::UNPATCH;
            break;
      case 'u':
        opts->uni = atoi(optarg);
        break;
      case 'h':
        opts->help = 1;
        break;
      case '?':
        break;
      default:
        break;
    }
  }
  return 0;
}


/*
 * help message for device info
 */
void DisplayDeviceInfoHelp(options *opts) {
  printf(
"Usage: %s [--plugin_id <plugin_id>]\n"
"\n"
"Show information on the devices loaded by llad.\n"
"\n"
"  -h, --help          Display this help message and exit.\n"
"  -p, --plugin_id <plugin_id> Show only devices owned by this plugin.\n"
"\n",
  opts->cmd.c_str());
}


/*
 * Display the Patch help
 */
void DisplayPatchHelp(options *opts) {
  printf(
"Usage: %s [--patch | --unpatch] --device <dev> --port <port> [--universe <uni>]\n"
"\n"
"Control lla port <-> universe mappings.\n"
"\n"
"  -a, --patch              Patch this port (default).\n"
"  -d, --device <device>    Id of device to patch.\n"
"  -h, --help               Display this help message and exit.\n"
"  -p, --port <port>        Id of the port to patch.\n"
"  -r, --unpatch            Unpatch this port.\n"
"  -u, --universe <uni>     Id of the universe to patch to (default 0).\n"
"\n",
  opts->cmd.c_str());
}


/*
 * help message for plugin info
 */
void DisplayPluginInfoHelp(options *opts) {
  printf(
"Usage: %s [--plugin_id <plugin_id>]\n"
"\n"
"Get info on the plugins loaded by llad. Called without arguments this will\n"
"display the plugins loaded by llad. When used with --plugin_id this wilk display\n"
"the specified plugin's description\n"
"\n"
"  -h, --help          Display this help message and exit.\n"
"  -p, --plugin_id <plugin_id>     Id of the plugin to fetch the description of.\n"
"\n",
  opts->cmd.c_str());
}


/*
 * help message for uni info
 */
void DisplayUniverseInfoHelp(options *opts) {
  printf(
"Usage: %s\n"
"\n"
"Shows info on the active universes in use.\n"
"\n"
"  -h, --help          Display this help message and exit.\n"
"\n",
  opts->cmd.c_str());
}


/*
 * Help message for set uni name
 */
void DisplayUniverseNameHelp(options *opts) {
  printf(
"Usage: %s --name <name> --universe <uni>\n"
"\n"
"Set a name for the specified universe\n"
"\n"
"  -h, --help               Display this help message and exit.\n"
"  -n, --name <name>        Name for the universe.\n"
"  -u, --universe <uni>     Id of the universe to name (default 0).\n"
"\n",
  opts->cmd.c_str()) ;
}


/*
 * Help message for set uni merge mode
 */
void DisplayUniverseMergeHelp(options *opts) {
  printf(
"Usage: %s --universe <uni> [ --ltp]\n"
"\n"
"Change the merge mode for the specified universe. Without --ltp it will\n"
"revert to HTP mode.\n"
"\n"
"  -h, --help               Display this help message and exit.\n"
"  -l, --ltp                Change to ltp mode.\n"
"  -u, --universe <uni>     Id of the universe to change.\n"
"\n",
  opts->cmd.c_str()) ;
}



/*
 * Help message for set dmx
 */
void display_set_dmx_help(options *opts) {
  printf(
"Usage: %s --universe <universe> --dmx 0,255,0,255\n"
"\n"
"Sets the DMX values for a universe.\n"
"\n"
"  -h, --help                      Display this help message and exit.\n"
"  -u, --universe <universe>       Universe number.\n"
"  -x, --dmx <values>              Comma separated DMX values.\n"
"\n",
  opts->cmd.c_str()) ;
}


/*
 * Display the help message
 */
void DisplayHelpAndExit(options *opts) {
  switch (opts->m) {
    case DEVICE_INFO:
      DisplayDeviceInfoHelp(opts);
      break;
    case DEVICE_PATCH:
      DisplayPatchHelp(opts);
      break;
    case PLUGIN_INFO:
      DisplayPluginInfoHelp(opts);
      break;
    case UNIVERSE_INFO:
      DisplayUniverseInfoHelp(opts);
      break;
    case UNIVERSE_NAME:
      DisplayUniverseNameHelp(opts);
      break;
    case UNI_MERGE:
      DisplayUniverseMergeHelp(opts);
      break;
    case SET_DMX:
      display_set_dmx_help(opts);
      break;
  }
  exit(0);
}


/*
 * Send a fetch device info request
 * @param client  the lla client
 * @param opts  the options
 */
int FetchDeviceInfo(LlaClient *client, options *opts) {
  if (opts->plugin_id > 0 && opts->plugin_id < LLA_PLUGIN_LAST)
    client->FetchDeviceInfo((lla_plugin_id) opts->plugin_id);
  else
    client->FetchDeviceInfo();
  return 0;
}


int Patch(LlaClient *client, options *opts) {
  if (opts->device_id == -1 || opts->port_id == -1) {
    DisplayPatchHelp(opts);
    exit(1);
  }

  client->Patch(opts->device_id, opts->port_id, opts->patch_action, opts->uni);
}


/*
 * Fetch information on plugins.
 */
int FetchPluginInfo(LlaClient *client, options *opts) {
  if (opts->plugin_id > 0 && opts->plugin_id < LLA_PLUGIN_LAST)
    client->FetchPluginInfo((lla_plugin_id) opts->plugin_id, true);
  else
    client->FetchPluginInfo();
  return 0;
}



/*
 * send a set name request
 * @param client the lla client
 * @param opts  the options
 */
int SetUniverseName(LlaClient *client, options *opts) {
  if (opts->uni == -1) {
    DisplayUniverseNameHelp(opts);
    exit(1) ;
  }

  client->SetUniverseName(opts->uni, opts->uni_name);
  return 0;
}


/*
 * send a set name request
 * @param client the lla client
 * @param opts  the options
 */
int SetUniverseMergeMode(LlaClient *client, options *opts) {
  if (opts->uni == -1) {
    DisplayUniverseMergeHelp(opts);
    exit(1) ;
  }

  client->SetUniverseMergeMode(opts->uni, opts->merge_mode);
  return 0;
}



/*
 * Send a dmx message
 * @param client the lla client
 * @param opts  the options
 */
int SendDmx(LlaClient *client, options *opts) {
  int i = 0;
  char *s;
  uint8_t buf[512];
  char *str = strdup(opts->dmx.c_str());

  if (opts->uni < 0) {
    display_set_dmx_help(opts) ;
    exit(1);
  }

  for (s = strtok(str, ","); s != NULL; s = strtok(NULL, ",")) {
    int v  = atoi(s) ;
    buf[i++] = v > 255 ? 255 : v;
  }

  if(i > 0)
    if (client->SendDmx(opts->uni, buf, i)) {
      printf("Send DMX failed:\n") ;
      return 1;
    }

  free(str);
  return 0;
}


/*
 *
 */
int main(int argc, char*argv[]) {
  SimpleClient lla_client;
  options opts;

  InitOptions(&opts);
  opts.cmd = argv[0];

  // decide how we should behave
  SetMode(&opts);

  if (opts.m == DEVICE_PATCH)
    ParsePatchOptions(argc, argv, &opts);
  else
    ParseOptions(argc, argv, &opts);


  if (opts.help)
    DisplayHelpAndExit(&opts);

  if (!lla_client.Setup()) {
    printf("error: %s\n", strerror(errno));
    exit(1);
  }

  LlaClient *client = lla_client.GetClient();
  SelectServer *ss = lla_client.GetSelectServer();

  Observer observer(&opts, ss);
  client->SetObserver(&observer);

  switch (opts.m) {
    case DEVICE_INFO:
      FetchDeviceInfo(client, &opts);
      break;
    case DEVICE_PATCH:
      Patch(client, &opts);
    case PLUGIN_INFO:
      FetchPluginInfo(client, &opts);
      break;
    case UNIVERSE_INFO:
      client->FetchUniverseInfo();
      break;
    case UNIVERSE_NAME:
      SetUniverseName(client, &opts);
      break;
    case UNI_MERGE:
      SetUniverseMergeMode(client, &opts);
      break;
    case SET_DMX:
      SendDmx(client, &opts);
      break;
  }

  ss->Run();
  return 0;
}
