Vagrant.configure("2") do |config|
  if Vagrant.has_plugin?("vagrant-proxyconf")
    config.proxy.http     = "http://proxy.s1n:3130"
    config.proxy.https    = "http://proxy.s1n:3130"
    config.proxy.no_proxy = "localhost,127.0.0.1"
  end

  config.vm.box = "debian/bullseye64"

  config.vm.box_check_update = false

  config.vm.define "nhi-dev"

  config.vm.synced_folder "./nhi", "/home/vagrant/nhi"

  config.vm.network "private_network", type: "dhcp"

  config.vm.disk :disk, size: "40GB", primary: true

  config.vm.provider "virtualbox" do |vb|
    vb.memory = "2048"
  end

  config.vm.provider "libvirt" do |lv|
    lv.memory = 2048
  end

  config.vm.provision "shell", inline: <<-'SHELL'
    # Avoid user interaction for apt-get commands
    export DEBIAN_FRONTEND=noninteractive

    # More timeout

    # Synchronize the package index files
    apt-get update

    # install nhi for terminal logging
    apt-get install -y binutils gawk sqlite3 libsqlite3-dev libbpf-dev git curl jq make clang vim gcc

    # install golang
    wget -P /tmp/ https://go.dev/dl/go1.19.3.linux-amd64.tar.gz
    tar -C /usr/local -xzf /tmp/go1.19.3.linux-amd64.tar.gz
    chmod 755 /usr/local/go/bin/
    echo 'export PATH=$PATH:/usr/local/go/bin' >> /etc/profile
    PATH=$PATH:/usr/local/go/bin

    # install nhi
    su vagrant -c 'cd /home/vagrant/nhi/; make; sudo make install'
    systemctl enable nhid
    echo "source /etc/nhi/nhi.bash" > /home/vagrant/.bashrc

  SHELL

  config.vm.provision :reload

end
