# REDHAWK Basic Components rh.psd
 
## Description

Contains the source and build script for the REDHAWK Basic Components rh.psd.
FFT-based power spectral density (PSD) component that transforms data from the
time domain to the frequency domain.  Output data is framed data where each
frame contains the frequncy domain representation of a subsection of the input.

## Branches and Tags

All REDHAWK core assets use the same branching and tagging policy. Upon release,
the `master` branch is rebased with the specific commit released, and that
commit is tagged with the asset version number. For example, the commit released
as version 1.0.0 is tagged with `1.0.0`.

Development branches (i.e. `develop` or `develop-X.X`, where *X.X* is an asset
version number) contain the latest unreleased development code for the specified
version. If no version is specified (i.e. `develop`), the branch contains the
latest unreleased development code for the latest released version.

## REDHAWK Version Compatibility

| Asset Version | Minimum REDHAWK Version Required |
| ------------- | -------------------------------- |
| 2.x           | 2.0                              |
| 1.x           | 1.10                             |

## Installation Instructions
This asset requires the rh.dsp and rh.fftlib shared libraries. These must be
installed in order to build and run this asset. To build from source, run the
`build.sh` script found at the top level directory. To install to $SDRROOT, run
`build.sh install`.

## Copyrights

This work is protected by Copyright. Please refer to the
[Copyright File](COPYRIGHT) for updated copyright information.

## License

REDHAWK Basic Component rh.psd is licensed under the GNU General Public License
(GPL).
