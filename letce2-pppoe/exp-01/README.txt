client side
# rfc4938client -f CONFIG_FILE

server side
# pppoe-server -I lan0 -S rfc4938 -Q /tmp/pppoe/sbin/rfc4938pppoe -L 11.0.0.X


/etc/ppp/pppoe-server-options
lock
noauth
passive
debug
noipdefault
nodeflate
noccp
nobsdcomp
nopcomp
ipcp-accept-local
ipcp-accept-remote
~                                  
