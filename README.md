pdffigures
==========
pdffigures is a command line tool that can be used to extract figures, tables, and captions from scholarly documents. See the [project website]{http://pdffigures.allenai.org}.

### Usage
1. Compile the command line tools:

```make DEBUG=0```

2. Run on a new PDF document and display the results:

```pdffigures -f /path/to/pdf```

See ```pdffigures -help``` for a list of additional command line arguements.

### Dependencies
pdffigures requires [leptonica](http://www.leptonica.com/) and [poppler](http://poppler.freedesktop.org/) to be installed. On MAC both of these dependencies can be installl through homebrew:

```brew install leptonica poppler```

On Ubuntu these dependencies can be install through apt-get:

```sudo apt-get install libpoppler-dev libleptonica-dev```

pdffigures uses std::regex, therefore compiling on Ubuntu requires g++ >= 4.9

### Support
pdffigures has been tested on MAC OS X 10.9 and Ubuntu 14.04, Windows is not supported.