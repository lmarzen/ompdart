# OMPDart
OMPDart - OpenMP Data Reduction Tool

OMPDart is a C/C++ static analysis tool for automatically generating efficient OpenMP GPU data mapping.


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
cd evaluation
bash download_dataset.sh
```

The `run_all.sh` script will run each version of each benchmark and tabulate the results.
```bash
bash run_all.sh
```

We provide the tool generated mappings in this repository, see source files with _ompdart in the file name. You can regenerate these mappings with `generate_ompdart_mappings.sh` which will run OMPDart on each benchmark and generate source code files with the .new extension.
```bash
bash generate_ompdart_mappings.sh
```

