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

```bash
cd evaluation
```

We provide the tool generated mappings in this repository, see source files with _ompdart in the file name. Tool-generated mappings can generated with `generate_ompdart_mappings.sh` which will run OMPDart on each benchmark and generate source code files with the .new extension.
```bash
bash generate_ompdart_mappings.sh
```

Benchmarks used for evaluation are in the sub directory `evaluation`. Some of these require data sets from the Rodinia suite.
Running the following command will automatically download and place the data sets in the correct path `evaluation/data`
```bash
bash download_dataset.sh
```

The `run_all.sh` script will build and run each version of each benchmark to gather execution time results.
```bash
bash run_all.sh
```

The `profile_all.sh` script will build and profile each version of each benchmark with NVIDIA Nsight Systems, used to results on data transfer time and CUDA memcpy calls.
```bash
bash profile_all.sh
```

### Citation

If you use this code for your research, please cite the following work:

> Luke Marzen, Akash Dutta, and Ali Jannesari. 2024. Static Generation of Efficient OpenMP Offload Data Mappings. In Proceedings of the International Conference for High Performance Computing, Networking, Storage, and Analysis (SC '24). IEEE Press, Article 35, 1–15. https://doi.org/10.1109/SC41406.2024.00041

'''tex
@inproceedings{10.1109/SC41406.2024.00041,
  author = {Marzen, Luke and Dutta, Akash and Jannesari, Ali},
  title = {Static Generation of Efficient OpenMP Offload Data Mappings},
  month = {Nov.},
  year = {2024},
  isbn = {9798350352917},
  publisher = {IEEE Press},
  url = {https://doi.org/10.1109/SC41406.2024.00041},
  doi = {10.1109/SC41406.2024.00041},
  booktitle = {Proceedings of the International Conference for High Performance Computing, Networking, Storage, and Analysis},
  articleno = {35},
  numpages = {15},
  location = {Atlanta, GA, USA},
  series = {SC '24}
}
'''
