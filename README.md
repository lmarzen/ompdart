# OMPDart
OMPDart - OpenMP Data Reduction Tool

An OpenMP GPU Data Mapping and Reuse Analysis Tool


### Usage

Dependencies:
- Clang 16+
- Boost C++ Libraries

To build OMPDart run the following script.
```bash
bash build.sh
```

Run OMPDart on a C/C++ source code file with OpenMP offload directives (but without target data mapping constructs). The transformed code with data mappings will be output into `<output_file>`.
```bash
bash run.sh -i <input_file> -o <output_file>
```


### Evaluation

Benchmarks used for evaluation are in the sub directory `evaluation`. Some of these require data sets from the Rodinia suite.
Running the following command will automatically download and place the data sets in the correct path `evaluation/data`
```bash
bash download_dataset.sh
```
