# Use an official Python runtime as a parent image
FROM python:3.9-slim

# Set the working directory in the container
WORKDIR /usr/src/app

# Install poetry
RUN pip install poetry

# Copy the dependency files
# We copy these first to leverage Docker's layer caching.
# Dependencies will only be re-installed if these files change.
COPY ../pyproject.toml ../poetry.lock* ./

# Install project dependencies
# --no-root is used because we are installing dependencies for a script, not a package
# to be installed itself.
RUN poetry install --no-root

# Copy the application code into the container
COPY . .

# Copy the account pool configuration file
COPY account_pool.json .

# Command to run the application
CMD ["python", "provisioner.py"]
