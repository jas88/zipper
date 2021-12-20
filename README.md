# Zipper

A simple tool for compressing and removing files from subdirectories.

Carefully designed to recover gracefully from interruption and make best use of multiple CPU cores.

This is quite a special-purpose tool, written to restructure the downloaded DICOM files for each patient in a PACS feed:

Each directory has an all-numerical name (the patient ID), and contains only files (no subdirectories).

In our use-case, this has created more than 35 million individual files within 86,000 directories, and Windows is struggling. (Yes, a poor choice of platform for the job, but it's the one we had...)

So: re-pack each patient's data into a 7z file per patient. This will turn 35 million files into 86,000: dramatically more manageable.

For each _numerical_ directory 123:

1 If an output file 123.zip already exists, skip to step 5
2 Create a subdirectory tmp123.zip
3 Copy the files within 123 to tmp123.zip
4 Rename tmp123.zip to 123.zip
5 Delete the files within 123
6 Move the completed ZIP file to a date-named subdirectory e.g. 2021-12-25

Interruption (crash, system failure etc) at any stage can be gracefully resumed: 123.zip already exists iff the directory 123 had already been fully processed, meaning either interruption occurred during the deletion step, or deletion failed (sharing violation, permissions error etc).

