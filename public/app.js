const connectionStatus =
  document.querySelector("#connection-status");

const jobsContainer =
  document.querySelector("#jobs");

const createJobForm =
  document.querySelector("#create-job-form");

const createJobStatus =
  document.querySelector("#create-job-status");

const createJobButton =
  createJobForm.querySelector(
    "button[type='submit']"
  );

function addJobDetail(
  details,
  label,
  value
) {
  const term =
    document.createElement("dt");

  term.textContent = label;

  const description =
    document.createElement("dd");

  description.textContent =
    String(value);

  details.append(
    term,
    description
  );
}

function optionalText(value) {
  if (value === null || value === "") {
    return "—";
  }

  return value;
}

function formatAvailableAt(milliseconds) {
  if (milliseconds === null) {
    return "—";
  }

  return new Date(
    milliseconds
  ).toLocaleString();
}

function createJobElement(job) {
  const article =
    document.createElement("article");

  article.className = "job-card";

  const heading =
    document.createElement("h3");

  heading.textContent =
    `${job.id}: ${job.name}`;

  const status =
    document.createElement("p");

  status.classList.add(
    "job-status",
    `job-status-${job.status}`
  );

  status.textContent = job.status;

  const details =
    document.createElement("dl");

  addJobDetail(
    details,
    "Payload",
    optionalText(job.payload)
  );

  addJobDetail(
    details,
    "Attempts",
    `${job.attemptsMade}/${job.maxAttempts}`
  );

  addJobDetail(
    details,
    "Priority",
    job.priority
  );

  addJobDetail(
    details,
    "Initial delay",
    `${job.delayMs} ms`
  );

  addJobDetail(
    details,
    "Retry backoff",
    `${job.retryBackoffMs} ms`
  );

  addJobDetail(
    details,
    "Available at",
    formatAvailableAt(job.availableAtMs)
  );

  addJobDetail(
    details,
    "Result",
    optionalText(job.result)
  );

  addJobDetail(
    details,
    "Failure reason",
    optionalText(job.failureReason)
  );

  article.append(
    heading,
    status,
    details
  );

  return article;
}

async function loadJobs() {
  try {
    const response =
      await fetch("/jobs");

    if (!response.ok) {
      throw new Error(
        `Server returned HTTP ${response.status}`
      );
    }

    const body =
      await response.json();

    jobsContainer.replaceChildren();

    for (const job of body.jobs) {
      jobsContainer.append(
        createJobElement(job)
      );
    }

    if (body.jobs.length === 0) {
      const message =
        document.createElement("p");

      message.textContent =
        "No jobs have been created.";

      jobsContainer.append(message);
    }

    connectionStatus.textContent =
      `Loaded ${body.count} job(s).`;
  } catch (error) {
    connectionStatus.textContent =
      `Cannot load jobs: ${error.message}`;
  }
}

createJobForm.addEventListener(
  "submit",
  async (event) => {
    event.preventDefault();

    const formData =
      new FormData(createJobForm);

    const requestBody = {
      name: String(
        formData.get("name") ?? ""
      ).trim(),

      payload: String(
        formData.get("payload") ?? ""
      ),

      maxAttempts: Number(
        formData.get("maxAttempts")
      ),

      delayMs: Number(
        formData.get("delayMs")
      ),

      retryBackoffMs: Number(
        formData.get("retryBackoffMs")
      ),

      priority: Number(
        formData.get("priority")
      )
    };

    createJobButton.disabled = true;
    createJobStatus.textContent =
      "Creating job...";

    try {
      const response =
        await fetch("/jobs", {
          method: "POST",

          headers: {
            "Content-Type":
              "application/json"
          },

          body: JSON.stringify(
            requestBody
          )
        });

      const body =
        await response.json();

      if (!response.ok) {
        throw new Error(
          body.error ??
          `Server returned HTTP ${response.status}`
        );
      }

      createJobStatus.textContent =
        `Created job ${body.id}.`;

      createJobForm.reset();

      await loadJobs();
    } catch (error) {
      createJobStatus.textContent =
        `Cannot create job: ${error.message}`;
    } finally {
      createJobButton.disabled = false;
    }
  }
);

const refreshDelayMs = 2000;

async function refreshJobs() {
  await loadJobs();

  window.setTimeout(refreshJobs, refreshDelayMs);
}

refreshJobs();