from distutils.core import setup
import setuptools

with open("README.md", "r") as fh:
    long_description = fh.read()

setup(
    author_email='berkin.ulukutlu@cern.ch',
    author='Berkin Ulukutlu',
    url='https://github.com/bulukutlu/TPCQCVis',
    name='TPCQCVis',
    version='v0.1', 
    #packages=setuptools.find_packages(),
    packages=setuptools.find_packages(exclude=["scripts*", "tests*","*d.ts"]),
    license='Not defined yet. Most probably similar to ALICE (CERN) license',
    long_description=long_description,
    long_description_content_type="text/markdown",
    include_package_data=False,
    package_data={
    '': ['../*/*/*/*.ts']
    },
    install_requires=[
        'numpy',
        'pandas',
        'runtime',
        'requests',
        'rise', 
        'schedule',
        ##---------------------   graphics  dependencies
        'bokeh',
        # ----------------------   jupyter notebook dependencies
        'ipywidgets',
        'jupyter_contrib_nbextensions',
        'notebook==6.4.12',
        'traitlets==5.9.0'
        ##---------------------   google drive api dependencies (for daily reporting from emails)
        'google-api-python-client',
        'google-auth-httplib2',
        'google-auth-oauthlib',
    ]
)
