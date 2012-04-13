#include "clustering/administration/main/initial_join.hpp"

#include <sstream>

#include "concurrency/wait_any.hpp"
#include "logger.hpp"

initial_joiner_t::initial_joiner_t(
        connectivity_cluster_t *cluster_,
        connectivity_cluster_t::run_t *cluster_run,
        const std::set<peer_address_t> &peers) :
    cluster(cluster_),
    peers_not_heard_from(peers),
    subs(boost::bind(&initial_joiner_t::on_connect, this, _1), 0)
{
    rassert(!peers.empty());
    coro_t::spawn_sometime(boost::bind(&initial_joiner_t::main_coro, this, cluster_run, auto_drainer_t::lock_t(&drainer)));

    connectivity_service_t::peers_list_freeze_t freeze(cluster);
    subs.reset(cluster, &freeze);
    std::set<peer_id_t> already_connected = cluster->get_peers_list();
    for (std::set<peer_id_t>::iterator it = already_connected.begin(); it != already_connected.end(); it++) {
        on_connect(*it);
    }
}

static const int initial_retry_interval_ms = 200;
static const int max_retry_interval_ms = 1000 * 15;
static const float retry_interval_growth_rate = 1.5;
static const int grace_period_before_warn_ms = 1000 * 5;

void initial_joiner_t::main_coro(connectivity_cluster_t::run_t *cluster_run, auto_drainer_t::lock_t keepalive) {
    try {
        int retry_interval_ms = initial_retry_interval_ms;
        do {
            for (std::set<peer_address_t>::const_iterator it = peers_not_heard_from.begin(); it != peers_not_heard_from.end(); it++) {
                cluster_run->join(*it);
            }
            signal_timer_t retry_timer(retry_interval_ms);
            wait_any_t waiter(&retry_timer);
            if (grace_period_timer) {
                waiter.add(grace_period_timer.get());
            }
            wait_interruptible(&waiter, keepalive.get_drain_signal());
            retry_interval_ms = std::min(int(retry_interval_ms * retry_interval_growth_rate), max_retry_interval_ms);
        } while (!peers_not_heard_from.empty() && (!grace_period_timer || !grace_period_timer->is_pulsed()));
        if (!peers_not_heard_from.empty()) {
            std::stringstream stringstream;
            std::set<peer_address_t>::const_iterator it = peers_not_heard_from.begin();
            stringstream << it->ip.as_dotted_decimal() << ":" << it->port;
            for (it++; it != peers_not_heard_from.end(); it++) {
                stringstream << ", " << it->ip.as_dotted_decimal() << ":" << it->port;
            }
            logWRN("We were unable to connect to the following peer(s): %s\n", stringstream.str().c_str());
        }
    } catch (interrupted_exc_t) {
        /* ignore */
    }
}

void initial_joiner_t::on_connect(peer_id_t peer) {
    if (peer != cluster->get_me()) {
        peers_not_heard_from.erase(cluster->get_peer_address(peer));
        if (!some_connection_made.is_pulsed()) {
            some_connection_made.pulse();
            grace_period_timer.reset(new signal_timer_t(grace_period_before_warn_ms));
        }
    }
}

