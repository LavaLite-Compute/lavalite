
int sv[2];
socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
pid_t pid = fork();

if (pid == 0) {
    close(sv[0]);
    run_eauth_client(sv[1]); // or server
    exit(0);
}

close(sv[1]);
char buffer[512];
read(sv[0], buffer, sizeof(buffer));
// pass buffer to mbatchd or validate it
close(sv[0]);
}
