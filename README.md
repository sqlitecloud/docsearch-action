# SQLite Cloud Documentation Search

This GitHub Action automates the process of building a table for an out-of-the-box documentation website full-text search powered by SQLite Cloud.

## Features

- Easy setup and configuration
- Seamless integration with your documentation website
- Utilizes the power of SQLite Cloud for efficient full-text search

## Usage

To use this GitHub Action, follow these steps:

1. Create a new workflow file (e.g., `.github/workflows/docsearch.yml`) in your repository.
2. Add the following code to the workflow file:

```yaml
name: Documentation Search

on:
    push:
        branches:
            - main

jobs:
    build:
        runs-on: ubuntu-latest

        steps:
            - name: Checkout repository
                uses: actions/checkout@v2

            - name: Build SQLite Cloud database
                uses: sqlitecloud/sqlitecloud-docsearch-action@v1
                with:
                    project-string: ${{ secrets.PROJECT_STRING }}
                    path: path/to/your/docs
                    database: my-docs-search
```

3. Make sure you have a project on SQLite Cloud. If not, sign up for an account and create one.
4. Add the Project Connection String as a secret in your repository settings, named `PROJECT_STRING`.
5. Customize the `database` and `path` according to your needs.
    * `remove-astro-headers`: Set this input to `true` if you want to remove the Astro headers from your documentation files before building the SQLite Cloud documentation table.
    * `remove-titles`: Set this input to `true` if you want to remove the titles from your documentation files before building the SQLite Cloud documentation table.
    * if the `path` input isn't specified the workflow will search for every .md or .mdx file recursively from the root folder.
6. Commit and push the workflow file to your repository.


Now, whenever you push changes to the `main` branch, the GitHub Action will automatically update the table for your documentation website's full-text search using SQLite Cloud.

For more information and advanced configuration options, please refer to this article [SQLite Cloud Blog](https://sqlitecloud.io).
