{
    outputs = { self, nixpkgs, ...}: {

        defaultPackage.x86_64-linux = nixpkgs.legacyPackages.x86_64-linux.stdenv.mkDerivation {
            name = "ljsyslog";
            src = ./.;
            depsBuildBuild = [ 
              nixpkgs.legacyPackages.x86_64-linux.ragel 
              nixpkgs.legacyPackages.x86_64-linux.kconfig-frontends 
            ];
            installPhase = ''
              install -d --mode=0755 $out
              install -d --mode=0755 $out/bin
              install --mode=0755 ljsyslog.stripped $out/bin/ljsyslog
            '';
        };

        nixosModule = { pkgs, config, ... }: {

          services.journald.forwardToSyslog = true;
          systemd.services.ljsyslog = {
            wantedBy = [ "multi-user.target" ];
            after = [ "nats.service" ];
            requires = [ "nats.service" ];
            serviceConfig = {
              Type = "simple";
              Restart = "always";
              RestartSec = "1sec";
              ExecStart = "${self.defaultPackage.x86_64-linux}/bin/ljsyslog";
            };
          };

        };

    };
}
