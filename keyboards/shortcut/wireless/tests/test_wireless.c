// Run with ./run.sh; exercises the actual firmware sources with host UART/time.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../module.c"
#include "../wireless.c"
#include "../smsg.c"
static uint32_t now;
static uint8_t input[4096];
static unsigned input_pos, input_len, transmitted;
uint32_t sync_timer_read32(void) { return now; }
uint32_t sync_timer_elapsed32(uint32_t t) { return now - t; }
void uart_init(uint32_t baud) { (void)baud; }
// Optional simulated module: ACKs every received copy after a latency; can lose copies.
static bool sim_module;
static unsigned sim_latency, sim_lose, sim_pending;
static uint32_t sim_ack_at[256];
void uart_transmit(const uint8_t *data, uint16_t length) {
    (void)length; transmitted++;
    if (!sim_module || data[0] == 0x61) return;
    if (sim_lose) { --sim_lose; return; }
    sim_ack_at[sim_pending++] = now + sim_latency;
}
bool uart_available(void) { return input_pos < input_len; }
uint8_t uart_read(void) { return input[input_pos++]; }
void last_matrix_activity_trigger(void) {}
void set_transport(transport_t t) { (void)t; }
transport_t get_transport(void) { return TRANSPORT_WLS; }
void usb_remote_wakeup(void) {}
void lpwr_task(void) {}
lpwr_state_t lpwr_get_state(void) { return LPWR_NORMAL; }
static void reset(void) {
    now = 0; reconnect_timer = 0; rx_timer = 0; transmitted = input_pos = input_len = 0;
    md_init(); md_report_reset();
    md_info.state = MD_STATE_CONNECTED; wls_devs = DEVS_BT1;
}
static void feed(uint8_t a, uint8_t b, uint8_t c) {
    input_pos = 0; input_len = 3;
    input[0] = a; input[1] = b; input[2] = c;
    md_receive_msg_task();
}
static void parser(void) {
    reset();
    feed(MD_REV_CMD_DEVCTRL, MD_REV_CMD_DEVCTRL_DISCONNECTED, MD_REV_CMD_DEVCTRL + MD_REV_CMD_DEVCTRL_DISCONNECTED);
    assert(md_info.state == MD_STATE_DISCONNECTED);
    input_pos = 0; input_len = 1; input[0] = MD_REV_CMD_RAW;
    md_receive_msg_task(); assert(rx_count == 1);
    now += 21; md_receive_msg_task(); assert(rx_count == 0);
    feed(MD_REV_CMD_DEVCTRL, MD_REV_CMD_DEVCTRL_CONNECTED, MD_REV_CMD_DEVCTRL + MD_REV_CMD_DEVCTRL_CONNECTED);
    assert(md_info.state == MD_STATE_CONNECTED);
    // Fixed RAW framing in RC8: command + 32 payload bytes + checksum.
    input_pos=0; input_len=MD_RAW_SIZE+2; memset(input,0,input_len);
    input[0]=MD_REV_CMD_RAW; input[input_len-1]=MD_REV_CMD_RAW;
    md_receive_msg_task(); assert(rx_count==0);
    for (unsigned j=0; j<100000; ++j) {
        input_pos=0; input_len=1; input[0]=rand(); now+=j%7;
        md_receive_msg_task(); assert(rx_count < sizeof(md_rev_payload));
    }
}
static void retries(void) {
    reset(); md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION);
    for (unsigned i=0;i<200;++i) { now += MD_SNED_PKT_TIMEOUT + 1; md_send_pkt_task(); }
    assert(!smsg_is_busy()); assert(md_info.state==MD_STATE_DISCONNECTED);
    assert(smsg_retry==0);
    md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION); md_send_pkt_task();
    feed(0x61,0x0d,0x0a); md_send_pkt_task();
    assert(!smsg_is_busy()); assert(smsg_retry==0);
}
static void replay(void) {
    reset(); md_info.state=MD_STATE_DISCONNECTED;
    uint8_t down[8]={0,0,4}, up[8]={0};
    md_send_kb(down); md_send_kb(up); md_report_task();
    assert(!smsg_is_busy() && report_count==2);
    md_info.state=MD_STATE_CONNECTED; md_report_task();
    uint8_t frame[50]; assert(smsg_peek(frame)==10 && frame[3]==4); smsg_pop();
    assert(smsg_peek(frame)==10 && frame[3]==0); smsg_pop();
    md_info.state=MD_STATE_DISCONNECTED;
    for (unsigned i=0;i<100;++i) { md_send_kb(i%2 ? up : down); }
    assert(report_count==16); now+=5001; md_report_task(); assert(report_count==0);
    assert(smsg_peek(frame)==3 && frame[0]==MD_SND_CMD_DEVCTRL); smsg_pop();
    md_info.state=MD_STATE_CONNECTED; md_report_task();
    assert(smsg_peek(frame)==10 && frame[3]==0);
    reset(); uint8_t filler[3]={1,2,3};
    for (unsigned i=0;i<39;++i) { assert(smsg_push(filler,3)); }
    md_send_kb(down); md_send_kb(up); md_report_task(); assert(report_count==2);
    for (unsigned i=0;i<39;++i) { smsg_pop(); }
    md_report_task(); assert(report_count==0);
    assert(smsg_peek(frame)==10 && frame[3]==4); smsg_pop();
    assert(smsg_peek(frame)==10 && frame[3]==0);
    md_report_reset(); assert(!md_reports_pending());
}
static void nkro(void) {
    reset(); report_nkro_t keys={0};
    for (unsigned k=4;k<=9;++k) { keys.bits[k/8] |= 1U<<(k%8); }
    wireless_send_nkro(&keys); md_report_task();
    while(smsg_is_busy()) { smsg_pop(); }
    keys.bits[4/8] &= ~(1U<<(4%8)); keys.bits[10/8] |= 1U<<(10%8);
    wireless_send_nkro(&keys); md_report_task();
    uint8_t frame[50]; assert(smsg_peek(frame)==10);
    bool seen[256]={0};
    for(unsigned i=3;i<9;++i) { assert(frame[i]>=5 && frame[i]<=10); assert(!seen[frame[i]]); seen[frame[i]]=true; }
    smsg_pop(); assert(smsg_peek(frame)==16);
    for(unsigned i=1;i<15;++i) { assert(frame[i]==0); }
    // A 7th key must appear in overflow, then move into a freed 6KRO slot.
    while(smsg_is_busy()) { smsg_pop(); }
    keys.bits[11/8] |= 1U<<(11%8); wireless_send_nkro(&keys); md_report_task();
    smsg_pop(); smsg_peek(frame); assert(frame[1+11/8] & (1U<<(11%8)));
    while(smsg_is_busy()) { smsg_pop(); }
    keys.bits[5/8] &= ~(1U<<(5%8)); wireless_send_nkro(&keys); md_report_task();
    smsg_peek(frame); bool found=false; for(unsigned i=3;i<9;++i) { if(frame[i]==11) found=true; } assert(found);
    smsg_pop(); smsg_peek(frame); for(unsigned i=1;i<15;++i) { assert(frame[i]==0); }
}
static void recovery_and_switch(void) {
    reset(); uint8_t release[8]={0}, frame[50];
    md_send_kb(release); md_report_task(); assert(report_count==0);
    // All transitions are already in smsg when UART stops acknowledging.
    for(unsigned i=0;i<200;++i) { now+=MD_SNED_PKT_TIMEOUT+1; md_send_pkt_task(); }
    assert(!smsg_is_busy() && report_count==0 && resync_mask);
    now+=500; md_report_task();
    assert(smsg_peek(frame)==3 && frame[1]==MD_SND_CMD_DEVCTRL_BT1);
    smsg_pop();
    feed(MD_REV_CMD_DEVCTRL, MD_REV_CMD_DEVCTRL_CONNECTED, MD_REV_CMD_DEVCTRL + MD_REV_CMD_DEVCTRL_CONNECTED);
    md_report_task(); assert(smsg_peek(frame)==10 && frame[3]==0);
    // Neither queued input nor occupied NKRO slots may migrate to a new host.
    report_nkro_t keys={0}; keys.bits[0]=1U<<4;
    wireless_send_nkro(&keys); md_report_task();
    assert(temp_report_keyboard.keys[0]==4 && smsg_is_busy());
    wireless_devs_change(DEVS_BT1,DEVS_BT2,false);
    assert(report_count==0 && !resync_mask && !temp_report_keyboard.keys[0]);
    static const uint8_t release_commands[]={MD_SND_CMD_SEND_KB,MD_SND_CMD_SEND_NKRO,MD_SND_CMD_SEND_CONSUMER,MD_SND_CMD_SEND_SYSTEM,MD_SND_CMD_SEND_FN,MD_SND_CMD_SEND_MOUSE};
    for(unsigned i=0;i<6;++i) { unsigned len=smsg_peek(frame); assert(len && frame[0]==release_commands[i]); for(unsigned j=1;j<len-1;++j) { assert(frame[j]==0); } smsg_pop(); }
    assert(smsg_peek(frame)==3 && frame[1]==MD_SND_CMD_DEVCTRL_BT2); smsg_pop();
    assert(!smsg_is_busy());
    // A system release is a system frame, independent of its zero usage.
    report_extra_t extra={.report_id=REPORT_ID_SYSTEM,.usage=0x81};
    wireless_send_extra(&extra); extra.usage=0; wireless_send_extra(&extra);
    md_info.state=MD_STATE_CONNECTED; md_report_task();
    assert(smsg_peek(frame)==3 && frame[0]==MD_SND_CMD_SEND_SYSTEM && frame[1]==1); smsg_pop();
    assert(smsg_peek(frame)==3 && frame[0]==MD_SND_CMD_SEND_SYSTEM && frame[1]==0);
    reset(); char name[46]; memset(name,'a',sizeof(name));
    md_send_manufacturer(name,sizeof(name)); md_send_pkt_task();
    assert(transmitted==1); // max 49-byte metadata frame fits TX scratch buffer.
    md_recover(); assert(rx_count==0 && smsg_retry==0 && md_info.state==MD_STATE_DISCONNECTED);
}
static void control_only_recovery(void) {
    reset(); wls_devs=DEVS_BT1;
    md_info.state=MD_STATE_DISCONNECTED;
    md_send_devctrl(MD_SND_CMD_DEVCTRL_BT1);
    for(unsigned i=0;i<200;++i) { now+=MD_SNED_PKT_TIMEOUT+1; md_send_pkt_task(); }
    assert(!smsg_is_busy() && reconnect_pending);
    assert(report_count==0 && resync_mask==0);
    now+=500; md_report_task();
    uint8_t frame[50];
    assert(smsg_peek(frame)==3 && frame[1]==MD_SND_CMD_DEVCTRL_BT1);
    feed(MD_REV_CMD_DEVCTRL,MD_REV_CMD_DEVCTRL_CONNECTED,MD_REV_CMD_DEVCTRL+MD_REV_CMD_DEVCTRL_CONNECTED);
    assert(!reconnect_pending);
    // A failed USB control command must not trigger radio reconnect attempts.
    md_report_reset(); wls_devs=DEVS_USB; md_info.state=MD_STATE_DISCONNECTED;
    reconnect_pending=true; now+=500; md_report_task();
    assert(!smsg_is_busy());
}
static void diagnostic_counts(void) {
    reset();
    uint32_t bytes=md_get_rx_bytes(), valid=md_get_valid_count(), bad=md_get_bad_count();
    feed(MD_REV_CMD_MD_FW_VERSION,42,MD_REV_CMD_MD_FW_VERSION+42);
    assert(md_get_rx_bytes()==bytes+3 && md_get_valid_count()==valid+1 && md_get_version()==42);
    feed(MD_REV_CMD_MD_FW_VERSION,42,0);
    assert(md_get_rx_bytes()==bytes+6 && md_get_bad_count()==bad+1);
    uint32_t tx=md_get_tx_count(), ack=md_get_ack_count();
    md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION); md_send_pkt_task();
    assert(md_get_tx_count()==tx+1);
    feed(0x61,0x0d,0x0a);
    assert(md_get_ack_count()==ack+1);
}

static void duplicate_ack_alias(void) {
    reset();
    uint8_t frame[50];
    // Frame A times out once, so it is on the wire twice and owes two ACKs.
    md_send_devctrl(MD_SND_CMD_DEVCTRL_BT1);
    md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION);
    md_send_pkt_task();
    now += MD_SNED_PKT_TIMEOUT + 1; md_send_pkt_task(); md_send_pkt_task(); md_send_pkt_task();
    assert(frame_sends == 2 && smsg_get_state() == smsg_state_busy);
    // The first ACK completes A and B goes out in the same pass.
    feed(0x61, 0x0d, 0x0a); md_send_pkt_task();
    assert(smsg_peek(frame) == 3 && frame[1] == MD_SND_CMD_DEVCTRL_FW_VERSION);
    assert(stale_acks == 1 && smsg_get_state() == smsg_state_busy);
    // The retransmission's ACK must not be credited to B.
    feed(0x61, 0x0d, 0x0a); md_send_pkt_task();
    assert(stale_acks == 0 && smsg_is_busy() && smsg_get_state() == smsg_state_busy);
    // B's own ACK completes it.
    feed(0x61, 0x0d, 0x0a); md_send_pkt_task();
    assert(!smsg_is_busy());
    // An owed ACK that never arrives expires instead of eating later ACKs forever.
    md_send_devctrl(MD_SND_CMD_DEVCTRL_BT1);
    md_send_pkt_task();
    now += MD_SNED_PKT_TIMEOUT + 1; md_send_pkt_task(); md_send_pkt_task(); md_send_pkt_task();
    assert(frame_sends == 2);
    feed(0x61, 0x0d, 0x0a); md_send_pkt_task();
    assert(!smsg_is_busy() && stale_acks == 1);
    now += MD_STALE_ACK_WINDOW + 1;
    md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION);
    md_main_task();
    assert(stale_acks == 0);
    feed(0x61, 0x0d, 0x0a); md_send_pkt_task();
    assert(!smsg_is_busy());
}

// Device log: tx=20 retries=10 ack=10 rx=19 ACKs after only the first copy was lost.
static void lost_copy_no_cascade(void) {
    reset(); md_info.state = MD_STATE_CONNECTED;
    sim_module = true; sim_latency = 2; sim_lose = 1; sim_pending = 0;
    uint32_t retries_before = md_get_retry_count(), tx_before = md_get_tx_count();
    for (unsigned i = 0; i < 10; ++i) md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION);
    for (unsigned t = 0; t < 2000 && smsg_is_busy(); ++t) {
        now++;
        input_pos = input_len = 0;
        unsigned kept = 0;
        for (unsigned i = 0; i < sim_pending; ++i) {
            if (sim_ack_at[i] <= now) { memcpy(&input[input_len], "\x61\x0d\x0a", 3); input_len += 3; }
            else sim_ack_at[kept++] = sim_ack_at[i];
        }
        sim_pending = kept;
        md_receive_msg_task(); md_send_pkt_task();
    }
    assert(!smsg_is_busy());
    // One lost copy may cost one retransmission, not one per remaining frame.
    assert(md_get_retry_count() - retries_before <= 2);
    assert(md_get_tx_count() - tx_before <= 12);
    sim_module = false;
}

static void reconnect_backoff(void) {
    reset(); md_info.state = MD_STATE_DISCONNECTED; reconnect_pending = true;
    uint8_t frame[50];
    unsigned attempts = 0; uint32_t last = 0, gap = 0;
    for (unsigned t = 0; t < 60000; t += 10) {
        now = t; md_report_task();
        while (smsg_is_busy()) {
            smsg_peek(frame);
            if (frame[0] == MD_SND_CMD_DEVCTRL && frame[1] == MD_SND_CMD_DEVCTRL_BT1) {
                if (attempts) gap = now - last;
                last = now; ++attempts;
            }
            smsg_pop();
        }
    }
    // 0, 0.5, 1.5, 3.5, 7.5, 15.5 s, then every 10 s: far fewer than one per 500 ms.
    assert(attempts >= 8 && attempts <= 12 && gap == MD_RECONNECT_MAX);
    // Typing while disconnected returns to the short delay.
    uint8_t down[8] = {0, 0, 4};
    md_send_kb(down);
    now += MD_RECONNECT_MIN; md_report_task();
    assert(smsg_peek(frame) == 3 && frame[1] == MD_SND_CMD_DEVCTRL_BT1);
    // A connection resets the interval for the next outage.
    while (smsg_is_busy()) smsg_pop();
    feed(MD_REV_CMD_DEVCTRL, MD_REV_CMD_DEVCTRL_CONNECTED, MD_REV_CMD_DEVCTRL + MD_REV_CMD_DEVCTRL_CONNECTED);
    assert(reconnect_interval == MD_RECONNECT_MIN);
}

static void battery_inquiry_schedule(void) {
    reset();
    uint8_t frame[50];
    now = 1; wireless_task();
    assert(smsg_peek(frame) == 3 && frame[1] == MD_SND_CMD_DEVCTRL_INQVOL); // first query without waiting
    while (smsg_is_busy()) smsg_pop();
    smsg_set_state(smsg_state_free);
    now += WLS_INQUIRY_BAT_TIME - 1; wireless_task();
    assert(!smsg_peek(frame) || frame[1] != MD_SND_CMD_DEVCTRL_INQVOL);
    while (smsg_is_busy()) smsg_pop();
    smsg_set_state(smsg_state_free);
    now += 1; wireless_task();
    assert(smsg_peek(frame) == 3 && frame[1] == MD_SND_CMD_DEVCTRL_INQVOL);
}

static void late_ack_and_metadata(void) {
    reset();
    uint32_t retries_before = md_get_retry_count();
    uint32_t drops_before = md_get_drop_count();
    md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION);
    md_send_pkt_task();
    smsg_retry = MD_SEND_PKT_RETRY;
    now += MD_SNED_PKT_TIMEOUT + 1;
    md_send_pkt_task();
    assert(smsg_get_state() == smsg_state_retry);
    input_pos = 0;
    input_len = 3;
    memcpy(input, "\x61\x0d\x0a", input_len);
    md_main_task();
    assert(!smsg_is_busy() && md_info.state == MD_STATE_CONNECTED);
    assert(md_get_retry_count() == retries_before && md_get_drop_count() == drops_before);

    md_send_vpid(0x1234, 0xfedc);
    uint8_t frame[50];
    assert(smsg_peek(frame) == 6);
    assert(frame[1] == 0x34 && frame[2] == 0x12 && frame[3] == 0xdc && frame[4] == 0xfe);
    smsg_pop();
    char name[257];
    memset(name, 'a', sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    md_send_devinfo(name);
    assert(!smsg_is_busy());
}
static void queue_model(void) {
    reset(); uint8_t expected[40][50], length[40], data[50];
    unsigned head=0,count=0;
    for(unsigned i=0;i<1000000;++i) {
        if(count && rand()%100<47) {
            assert(smsg_peek(data)==length[head]); assert(!memcmp(data,expected[head],length[head]));
            smsg_pop(); head=(head+1)%40; count--;
        } else {
            unsigned size=1+rand()%50; for(unsigned j=0;j<size;++j) data[j]=rand();
            bool pushed=smsg_push(data,size); assert(pushed==(count<39));
            if(pushed) { unsigned tail=(head+count)%40; length[tail]=size; memcpy(expected[tail],data,size); count++; }
        }
    }
}
int main(void) {
    parser();
    retries();
    replay();
    nkro();
    recovery_and_switch();
    control_only_recovery();
    diagnostic_counts();
    late_ack_and_metadata();
    duplicate_ack_alias();
    lost_copy_no_cascade();
    reconnect_backoff();
    battery_inquiry_schedule();
    queue_model();
    puts("PASS wireless: parser, retry bound, late ACK, duplicate ACK, lost copy without cascade, reconnect backoff, battery inquiry schedule, replay, backpressure, NKRO, host switch, control-only recovery, metadata, 1M queue operations");
}
