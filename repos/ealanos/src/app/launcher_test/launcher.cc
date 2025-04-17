#include <libc/component.h>
#include <base/log.h>
#include <ealanos/laucher/connection.h>
#include <base/thread.h>
#include <base/env.h>

#include <timer_session/connection.h>


#define Cell(name) static const char *name = \
    "<start name=\"" #name "\" priority=\"-1\">\n" \
    "    <binary name=\"allocating_cell\"/>\n" \
    "    <resource name=\"RAM\" quantum=\"128M\"/>\n" \
    "    <config>\n" \
    "        <vfs> <dir name=\"dev\">\n" \
    "            <log/>\n" \
    "            <inline name=\"rtc\">2022-07-20 14:30</inline>\n" \
    "            </dir>\n" \
    "        </vfs>\n" \
    "        <libc stdout=\"/dev/log\" stderr=\"/dev/log\" rtc=\"/dev/rtc\"/>\n" \
    "    </config>\n" \
    "</start>\n";

Cell(allocator1);
Cell(allocator2);
Cell(allocator3);
Cell(allocator4);
Cell(allocator5);
Cell(allocator6);
Cell(allocator7);
Cell(allocator8);

static const char *cells[8] = {allocator1, allocator2, allocator3, allocator4, allocator5, allocator6, allocator7, allocator8};

void Libc::Component::construct(Libc::Env &env)
{
    Genode::Thread *myself = Genode::Thread::myself();
    Genode::Affinity::Location loc = myself->affinity();
    Genode::Affinity affinity(env.cpu().affinity_space(), loc);

    Timer::Connection _timer{env};

    Libc::with_libc([&]()
                    { 
                        Ealan::Launcher_connection _launcher{env, affinity};
                        for (int i = 0; i < 8; i++) {

                            bool repeat = false;
                            do
                            {
                                try {
                                    Genode::Xml_node xml(cells[i]);
                                    _launcher.launch(xml);
                                } catch (Genode::Xml_attribute::Invalid_syntax) {
                                    Genode::error("Invalid XML start node"); 
                                } catch (Genode::Ipc_error) {
                                    repeat = true;
                                }
                            } while (repeat);
                            //_timer.msleep(1000);
                        }
                        Genode::log("Launched dummy"); });
    while(true)
        ;
}