%global debug_package %{nil}

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
install -Dm644 %{_sourcedir}/packaging/linux/mercsport.desktop %{buildroot}%{_datadir}/applications/mercsport.desktop
install -Dm644 %{_sourcedir}/packaging/linux/mercsport.png %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/mercsport.png
mkdir -p %{buildroot}%{_docdir}/mercsport
install -m644 %{_sourcedir}/README.md %{_sourcedir}/FOLDER_SETUP.md %{_sourcedir}/THIRD_PARTY.md %{buildroot}%{_docdir}/mercsport/

%files
%{_bindir}/mercs
%{_datadir}/mercsport
%{_datadir}/applications/mercsport.desktop
%{_datadir}/icons/hicolor/256x256/apps/mercsport.png
%{_docdir}/mercsport
