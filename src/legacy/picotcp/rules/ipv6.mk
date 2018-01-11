OPTIONS+=-DPICO_SUPPORT_IPV6 -DPICO_SUPPORT_ICMP6
MOD_OBJ+=$(LIBBASE)/pico_ipv6.o $(LIBBASE)/pico_ipv6_nd.o $(LIBBASE)/pico_icmp6.o
include rules/ipv6frag.mk
