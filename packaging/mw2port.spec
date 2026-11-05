Name:           mercsport
Version:        %{_version}
Release:        1%{?dist}
Summary:        Native 64-bit port of MechWarrior 2: Mercenaries
License:        Proprietary
BuildRequires:  gcc, gcc-c++, make, pkgconf-pkg-config, SDL3-devel >= 3.2.0
Requires:       SDL3 >= 3.2.0, vulkan-loader
Recommends:     mesa-vulkan-drivers

%description
Native port of MechWarrior 2: Mercenaries, drawn with Vulkan through SDL3's GPU API.
Supports the retail DOS, dedicated 3dfx and Titanium Direct3D appearances using
your own original Mercenaries game data. No game assets are included.

%prep
# source is already checked out in the working directory

%build
make -C %{_sourcedir} -j$(nproc) mercs

%install
install -Dm755 %{_sourcedir}/mercs %{buildroot}%{_bindir}/mercs
mkdir -p %{buildroot}%{_datadir}/mercsport
cp -r %{_sourcedir}/assets %{buildroot}%{_datadir}/mercsport/

%files
%{_bindir}/mercs
%{_datadir}/mercsport
%doc %{_sourcedir}/README.md %{_sourcedir}/FOLDER_SETUP.md %{_sourcedir}/THIRD_PARTY.md
