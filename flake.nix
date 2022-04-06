{
    outputs = { self, nixpkgs, ...}: {

      packages.x86_64-linux.default = nixpkgs.lib.makeOverridable self.derivations.default {
        inherit (nixpkgs.legacyPackages.x86_64-linux.stdenv) mkDerivation;
        inherit (nixpkgs.legacyPackages.x86_64-linux) ragel kconfig-frontends;
      };

      derivations.default = {
        mkDerivation,
        ragel,
        kconfig-frontends,
        nats ? { host = "127.0.0.1"; port = "4222"; }
      }: 
      mkDerivation {
        name = "ljsyslog";
        src = ./.;
        depsBuildBuild = [ 
          ragel 
          kconfig-frontends 
        ];
        makeFlags = [ 
          "Q="
          "CONFIG_NATS_HOST=\"${nats.host}\""
          "CONFIG_NATS_PORT=\"${nats.port}\""
        ];
        installPhase = ''
          install -d --mode=0755 $out
          install -d --mode=0755 $out/bin
          install --mode=0755 ljsyslog.stripped $out/bin/ljsyslog
        '';
      };

    };
}
