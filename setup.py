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
    license='Not defined yet. Most probably similar to ALICE (CERN)  license',
    long_description=long_description,
    long_description_content_type="text/markdown",
    include_package_data=False,
    package_data={
    '': ['../*/*/*/*.ts']
    },
    install_requires=[
        'numpy',
        'pandas',
        ##---------------------   graphics  dependencies
        'bokeh==2.2.3',
        # ----------------------   jupyter notebook dependencies
        'ipywidgets',
        'runtime',
        'requests',
        'rise', 
        "notebook==5.7.8"
    ]
)
