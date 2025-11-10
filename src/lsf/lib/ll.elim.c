[VER][TYPE=HOSTS][N_HOSTS]
  [HOST][HLEN][host...][N_KV]
    [KEY][KLEN][key...][VAL][VLEN][val...]
    [KEY][KLEN][key...][VAL][VLEN][val...]
  [HOST][HLEN][host...][N_KV]
    ...

	u8 ver = get8(p); u8 type = get8(p);
u8 n_hosts = get8(p);
for (i=0; i<n_hosts; i++) {
    u16 hlen = get16(p); const char *host = getn(p, hlen);
    u16 n_kv  = get16(p);
    for (j=0; j<n_kv; j++) {
        u16 klen = get16(p); const char *key = getn(p, klen);
        u16 vlen = get16(p); const char *val = getn(p, vlen);
        report_host_kv(host, key, val);
    }
}

[VER][TYPE=FLOAT][N_KV]
  [KEY][KLEN][...][VAL][VLEN][...]
  ...

